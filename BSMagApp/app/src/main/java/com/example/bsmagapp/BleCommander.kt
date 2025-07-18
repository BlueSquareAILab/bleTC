package com.example.bsmagapp

import android.bluetooth.*
import android.content.Context
import android.os.Build
import android.os.Handler
import android.os.Looper
import android.util.Log
import java.util.*

class BleCommander(private val context: Context) {

    companion object {
        private const val TAG = "BleCommander"
        private const val SERVICE_UUID = "2ca354b0-5f62-11ef-b4d4-f7af9038ee7d"
        private const val CHARACTERISTIC_UUID = "35c34c80-5f62-11ef-b4d4-f7af9038ee7d"
        private const val CCCD_UUID = "00002902-0000-1000-8000-00805f9b34fb"
        private const val TIMEOUT_DURATION = 15000L // 전체 타임아웃 15초
        private const val REQUESTED_MTU = 517
    }

    private var bluetoothGatt: BluetoothGatt? = null
    private var targetCharacteristic: BluetoothGattCharacteristic? = null
    private val handler = Handler(Looper.getMainLooper())
    private var isConnecting = false
    private var currentCallback: BleCommandCallback? = null
    private var timeoutRunnable: Runnable? = null
    private var responseBuffer = StringBuilder()
    private var commandSent = false

    interface BleCommandCallback {
        fun onSuccess(response: String)
        fun onError(error: String)
        fun onProgress(message: String)
    }

    fun executeAmmoReset(device: BluetoothDevice, callback: BleCommandCallback) {
        Log.d(TAG, "=== executeAmmoReset START ===")

        if (isConnecting) {
            callback.onError("이미 연결 시도 중입니다")
            return
        }

        currentCallback = callback
        isConnecting = true
        responseBuffer.clear()
        commandSent = false

        callback.onProgress("연결 중...")
        Log.d(TAG, "Connecting to device: ${device.address}")

        // 전체 프로세스 타임아웃 15초
        timeoutRunnable = Runnable {
            Log.e(TAG, "*** PROCESS TIMEOUT ***")
            val currentBuffer = responseBuffer.toString()
            currentCallback?.onError("시간 초과 (15초)\n응답: $currentBuffer")
            cleanup()
        }
        handler.postDelayed(timeoutRunnable!!, TIMEOUT_DURATION)

        try {
            bluetoothGatt = device.connectGatt(context, false, gattCallback, BluetoothDevice.TRANSPORT_LE)
        } catch (e: Exception) {
            Log.e(TAG, "Connection failed", e)
            cleanup()
            callback.onError("연결 실패: ${e.message}")
        }
    }

    private val gattCallback = object : BluetoothGattCallback() {
        override fun onConnectionStateChange(gatt: BluetoothGatt?, status: Int, newState: Int) {
            Log.d(TAG, "Connection state: status=$status, newState=$newState")

            when (newState) {
                BluetoothProfile.STATE_CONNECTED -> {
                    Log.d(TAG, "Connected to GATT server")
                    handler.post {
                        currentCallback?.onProgress("MTU 요청 중...")
                    }

                    // MTU 요청
                    handler.postDelayed({
                        try {
                            gatt?.requestMtu(REQUESTED_MTU)
                        } catch (e: Exception) {
                            Log.e(TAG, "MTU request failed", e)
                            handler.post {
                                currentCallback?.onError("MTU 요청 실패")
                                cleanup()
                            }
                        }
                    }, 500)
                }
                BluetoothProfile.STATE_DISCONNECTED -> {
                    Log.d(TAG, "Disconnected from GATT server")
                    cleanup()
                }
            }
        }

        override fun onMtuChanged(gatt: BluetoothGatt?, mtu: Int, status: Int) {
            Log.d(TAG, "MTU changed: $mtu, status: $status")

            if (status == BluetoothGatt.GATT_SUCCESS) {
                handler.post {
                    currentCallback?.onProgress("서비스 검색 중...")
                }

                handler.postDelayed({
                    try {
                        gatt?.discoverServices()
                    } catch (e: Exception) {
                        Log.e(TAG, "Service discovery failed", e)
                        handler.post {
                            currentCallback?.onError("서비스 검색 실패")
                            cleanup()
                        }
                    }
                }, 500)
            } else {
                handler.post {
                    currentCallback?.onError("MTU 변경 실패")
                    cleanup()
                }
            }
        }

        override fun onServicesDiscovered(gatt: BluetoothGatt?, status: Int) {
            Log.d(TAG, "Services discovered: status=$status")

            if (status == BluetoothGatt.GATT_SUCCESS) {
                handler.post {
                    currentCallback?.onProgress("특성 검색 중...")
                }

                val service = gatt?.getService(UUID.fromString(SERVICE_UUID))
                if (service != null) {
                    val characteristic = service.getCharacteristic(UUID.fromString(CHARACTERISTIC_UUID))
                    if (characteristic != null) {
                        targetCharacteristic = characteristic
                        Log.d(TAG, "Target characteristic found")

                        handler.post {
                            currentCallback?.onProgress("알림 설정 중...")
                        }

                        setupNotification(gatt, characteristic)
                    } else {
                        handler.post {
                            currentCallback?.onError("특성을 찾을 수 없습니다")
                            cleanup()
                        }
                    }
                } else {
                    handler.post {
                        currentCallback?.onError("서비스를 찾을 수 없습니다")
                        cleanup()
                    }
                }
            } else {
                handler.post {
                    currentCallback?.onError("서비스 검색 실패")
                    cleanup()
                }
            }
        }

        override fun onDescriptorWrite(gatt: BluetoothGatt?, descriptor: BluetoothGattDescriptor?, status: Int) {
            Log.d(TAG, "Descriptor write: status=$status")

            if (status == BluetoothGatt.GATT_SUCCESS) {
                handler.post {
                    currentCallback?.onProgress("명령 전송 중...")
                }

                // 알림 설정 완료 후 명령 전송
                handler.postDelayed({
                    sendAmmoResetCommand()
                }, 1000)
            } else {
                handler.post {
                    currentCallback?.onError("알림 설정 실패")
                    cleanup()
                }
            }
        }

        override fun onCharacteristicWrite(
            gatt: BluetoothGatt?,
            characteristic: BluetoothGattCharacteristic?,
            status: Int
        ) {
            Log.d(TAG, "Characteristic write: status=$status")

            if (status == BluetoothGatt.GATT_SUCCESS) {
                commandSent = true
                handler.post {
                    currentCallback?.onProgress("응답 대기 중...")
                }
            } else {
                handler.post {
                    currentCallback?.onError("명령 전송 실패")
                    cleanup()
                }
            }
        }

        override fun onCharacteristicChanged(
            gatt: BluetoothGatt?,
            characteristic: BluetoothGattCharacteristic?
        ) {
            val targetUuid = targetCharacteristic?.uuid
            val receivedUuid = characteristic?.uuid

            if (receivedUuid == targetUuid) {
                val data = characteristic?.value
                if (data != null) {
                    val response = data.toString(Charsets.UTF_8)
                    Log.d(TAG, "Received response: '$response'")

                    responseBuffer.append(response)
                    val fullResponse = responseBuffer.toString()

                    // 명령 전송 후 받은 응답만 처리
                    if (commandSent) {
                        handler.post {
                            when {
                                fullResponse.contains("result: ok") -> {
                                    currentCallback?.onSuccess("탄창 리셋 완료!")
                                    cleanup()
                                }
                                fullResponse.contains("result: fail") -> {
                                    currentCallback?.onError("리셋 실패: $fullResponse")
                                    cleanup()
                                }
                                fullResponse.contains("ammo:") -> {
                                    currentCallback?.onSuccess("탄창 리셋 완료!")
                                    cleanup()
                                }
                                // 응답이 완료되지 않았으면 계속 대기
                            }
                        }
                    }
                }
            }
        }
    }

    private fun setupNotification(gatt: BluetoothGatt, characteristic: BluetoothGattCharacteristic) {
        try {
            val success = gatt.setCharacteristicNotification(characteristic, true)
            if (!success) {
                handler.post {
                    currentCallback?.onError("알림 설정 실패")
                    cleanup()
                }
                return
            }

            val descriptor = characteristic.getDescriptor(UUID.fromString(CCCD_UUID))
            if (descriptor == null) {
                handler.post {
                    currentCallback?.onError("디스크립터를 찾을 수 없습니다")
                    cleanup()
                }
                return
            }

            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
                gatt.writeDescriptor(descriptor, BluetoothGattDescriptor.ENABLE_NOTIFICATION_VALUE)
            } else {
                descriptor.value = BluetoothGattDescriptor.ENABLE_NOTIFICATION_VALUE
                gatt.writeDescriptor(descriptor)
            }
        } catch (e: Exception) {
            Log.e(TAG, "Notification setup failed", e)
            handler.post {
                currentCallback?.onError("알림 설정 실패")
                cleanup()
            }
        }
    }

    private fun sendAmmoResetCommand() {
        val command = "ammo reset"  // 개행문자 없이
        val bytes = command.toByteArray(Charsets.UTF_8)

        Log.d(TAG, "Sending command: '$command'")

        try {
            targetCharacteristic?.let { characteristic ->
                if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
                    val writeType = BluetoothGattCharacteristic.WRITE_TYPE_DEFAULT
                    bluetoothGatt?.writeCharacteristic(characteristic, bytes, writeType)
                } else {
                    characteristic.value = bytes
                    bluetoothGatt?.writeCharacteristic(characteristic)
                }
            } ?: run {
                Log.e(TAG, "Target characteristic is null")
                handler.post {
                    currentCallback?.onError("특성이 설정되지 않았습니다")
                    cleanup()
                }
            }
        } catch (e: Exception) {
            Log.e(TAG, "Command send failed", e)
            handler.post {
                currentCallback?.onError("명령 전송 실패")
                cleanup()
            }
        }
    }

    private fun disconnect() {
        try {
            bluetoothGatt?.disconnect()
        } catch (e: Exception) {
            Log.e(TAG, "Disconnect failed", e)
        }
    }

    private fun cleanup() {
        Log.d(TAG, "Cleaning up")
        isConnecting = false
        currentCallback = null
        targetCharacteristic = null
        responseBuffer.clear()
        commandSent = false

        timeoutRunnable?.let { handler.removeCallbacks(it) }

        try {
            bluetoothGatt?.close()
        } catch (e: Exception) {
            Log.e(TAG, "GATT close failed", e)
        }
        bluetoothGatt = null
    }

    fun isConnecting(): Boolean = isConnecting
}