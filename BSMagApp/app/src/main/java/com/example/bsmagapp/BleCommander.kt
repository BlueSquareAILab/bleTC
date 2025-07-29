package com.example.bsmagapp

import android.Manifest
import android.bluetooth.*
import android.content.Context
import android.os.Build
import android.os.Handler
import android.os.Looper
import android.util.Log
import androidx.annotation.RequiresPermission
import java.util.*

class BleCommander(private val context: Context) {

    companion object {
        private const val TAG = "BleCommander"
        private const val SERVICE_UUID = "2ca354b0-5f62-11ef-b4d4-f7af9038ee7d"
        private const val CHARACTERISTIC_UUID = "35c34c80-5f62-11ef-b4d4-f7af9038ee7d"
        private const val CCCD_UUID = "00002902-0000-1000-8000-00805f9b34fb"
        private const val TIMEOUT_DURATION = 15000L // 전체 타임아웃 15초
        private const val RESPONSE_TIMEOUT = 5000L   // 응답 타임아웃 5초
        private const val REQUESTED_MTU = 517
    }

    private var bluetoothGatt: BluetoothGatt? = null
    private var targetCharacteristic: BluetoothGattCharacteristic? = null
    private val handler = Handler(Looper.getMainLooper())
    private var isConnecting = false
    private var currentCallback: BleCommandCallback? = null
    private var timeoutRunnable: Runnable? = null
    private var responseTimeoutRunnable: Runnable? = null
    private var responseBuffer = StringBuilder()
    private var commandSent = false
    private var currentCommand = ""
    private var configWriteStep = 0  // config write 단계 추적용

    interface BleCommandCallback {
        fun onSuccess(response: String)
        fun onError(error: String)
        fun onProgress(message: String)
    }

    @RequiresPermission(Manifest.permission.BLUETOOTH_CONNECT)
    fun executePulseTest(device: BluetoothDevice, callback: BleCommandCallback) {
        Log.d(TAG, "=== executePulseTest START ===")
        executeCommand(device, "pulse", callback)
    }

    @RequiresPermission(Manifest.permission.BLUETOOTH_CONNECT)
    fun executeConfigDump(device: BluetoothDevice, callback: BleCommandCallback) {
        Log.d(TAG, "=== executeConfigDump START ===")
        executeCommand(device, "config dump", callback)
    }

    @RequiresPermission(Manifest.permission.BLUETOOTH_CONNECT)
    fun executeAmmoReset(device: BluetoothDevice, callback: BleCommandCallback) {
        Log.d(TAG, "=== executeAmmoReset START ===")
        executeCommand(device, "ammo reset", callback)
    }

    @RequiresPermission(Manifest.permission.BLUETOOTH_CONNECT)
    fun executeConfigWrite(device: BluetoothDevice, debounceDelay: Int, maxAmmo: Int, callback: BleCommandCallback) {
        Log.d(TAG, "=== executeConfigWrite START ===")
        Log.d(TAG, "debounceDelay: $debounceDelay, maxAmmo: $maxAmmo")
        
        // 특별한 명령어 형태로 저장 (나중에 파싱해서 두 개의 명령으로 분리)
        executeCommand(device, "config_write_$debounceDelay:$maxAmmo", callback, 20000L)
    }

    @RequiresPermission(Manifest.permission.BLUETOOTH_CONNECT)
    private fun executeCommand(
        device: BluetoothDevice, 
        command: String, 
        callback: BleCommandCallback,
        timeout: Long = TIMEOUT_DURATION
    ) {
        if (isConnecting) {
            callback.onError("이미 연결 시도 중입니다")
            return
        }

        currentCallback = callback
        isConnecting = true
        responseBuffer.clear()
        commandSent = false
        currentCommand = command
        configWriteStep = 0  // config write 단계 초기화

        callback.onProgress("연결 중...")
        Log.d(TAG, "Connecting to device: ${device.address} for command: $command")

        // 전체 프로세스 타임아웃 설정
        timeoutRunnable = Runnable {
            Log.e(TAG, "*** PROCESS TIMEOUT for command: $command ***")
            val currentBuffer = responseBuffer.toString()
            currentCallback?.onError("시간 초과 (${timeout/1000}초)\n응답: $currentBuffer")
            cleanup()
        }
        handler.postDelayed(timeoutRunnable!!, timeout)

        try {
            bluetoothGatt = device.connectGatt(context, false, gattCallback, BluetoothDevice.TRANSPORT_LE)
        } catch (e: Exception) {
            Log.e(TAG, "Connection failed", e)
            cleanup()
            callback.onError("연결 실패: ${e.message}")
        }
    }

    private val gattCallback = object : BluetoothGattCallback() {
        
        @RequiresPermission(Manifest.permission.BLUETOOTH_CONNECT)
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

        @RequiresPermission(Manifest.permission.BLUETOOTH_CONNECT)
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

        @RequiresPermission(Manifest.permission.BLUETOOTH_CONNECT)
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

        @RequiresPermission(Manifest.permission.BLUETOOTH_CONNECT)
        override fun onDescriptorWrite(gatt: BluetoothGatt?, descriptor: BluetoothGattDescriptor?, status: Int) {
            Log.d(TAG, "Descriptor write: status=$status")

            if (status == BluetoothGatt.GATT_SUCCESS) {
                handler.post {
                    currentCallback?.onProgress("명령 전송 중...")
                }

                // 알림 설정 완료 후 명령 전송
                handler.postDelayed({
                    sendCommand()
                }, 1000)
            } else {
                handler.post {
                    currentCallback?.onError("알림 설정 실패")
                    cleanup()
                }
            }
        }

        @RequiresPermission(Manifest.permission.BLUETOOTH_CONNECT)
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
                
                // 명령 전송 성공 후 응답 타임아웃 시작
                startResponseTimeout()
            } else {
                handler.post {
                    currentCallback?.onError("명령 전송 실패")
                    cleanup()
                }
            }
        }

        @RequiresPermission(Manifest.permission.BLUETOOTH_CONNECT)
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
                        // 응답을 받았으므로 응답 타임아웃 취소
                        cancelResponseTimeout()
                        
                        // 응답 처리
                        handleResponse(fullResponse)
                    }
                }
            }
        }
    }

    @RequiresPermission(Manifest.permission.BLUETOOTH_CONNECT)
    private fun handleResponse(fullResponse: String) {
        handler.post {
            when {
                currentCommand == "ammo reset" -> handleAmmoResetResponse(fullResponse)
                currentCommand == "pulse" -> handlePulseResponse(fullResponse)
                currentCommand == "config dump" -> handleConfigDumpResponse(fullResponse)
                currentCommand.startsWith("config_write_") -> handleConfigWriteResponse(fullResponse)
                else -> {
                    Log.w(TAG, "Unknown command response: $currentCommand")
                    currentCallback?.onError("알 수 없는 명령어")
                    cleanup()
                }
            }
        }
    }

    private fun handleAmmoResetResponse(response: String) {
        when {
            response.contains("result: ok") -> {
                currentCallback?.onSuccess("탄창 리셋 완료!")
                cleanup()
            }
            response.contains("result: fail") -> {
                currentCallback?.onError("리셋 실패: $response")
                cleanup()
            }
            response.contains("ammo:") -> {
                currentCallback?.onSuccess("탄창 리셋 완료!")
                cleanup()
            }
        }
    }

    private fun handlePulseResponse(response: String) {
        when {
            response.contains("result: ok") -> {
                currentCallback?.onSuccess("Pulse 테스트 성공!")
                cleanup()
            }
            response.contains("result: fail") -> {
                currentCallback?.onError("Pulse 테스트 실패: $response")
                cleanup()
            }
            response.contains("pulse:") || response.length > 10 -> {
                currentCallback?.onSuccess("Pulse 테스트 완료: $response")
                cleanup()
            }
        }
    }

    @RequiresPermission(Manifest.permission.BLUETOOTH_CONNECT)
    private fun handleConfigDumpResponse(response: String) {
        Log.d(TAG, "Config dump response received: $response")
        when {
            response.contains("result: ok") || 
            response.contains("Config dump:") || 
            response.length > 10 -> {
                // config dump 응답 전체를 로그에 출력
                Log.i(TAG, "=== CONFIG DUMP RESPONSE ===")
                Log.i(TAG, response)
                Log.i(TAG, "=== END CONFIG DUMP ===")
                // 전체 응답을 성공 메시지로 반환
                currentCallback?.onSuccess(response)
                cleanup()
            }
            response.contains("result: fail") -> {
                currentCallback?.onError("Config dump 실패: $response")
                cleanup()
            }
        }
    }

    @RequiresPermission(Manifest.permission.BLUETOOTH_CONNECT)
    private fun handleConfigWriteResponse(response: String) {
        Log.d(TAG, "Config write response received: $response")
        Log.d(TAG, "Current configWriteStep: $configWriteStep")
        
        when {
            response.contains("result: ok") -> {
                if (configWriteStep == 0) {
                    // 첫 번째 명령 성공 (debounceDelay)
                    Log.d(TAG, "First command (debounceDelay) successful, sending second command")
                    configWriteStep = 1  // 다음 단계로
                    commandSent = false  // 다시 명령 전송 가능하도록
                    
                    handler.postDelayed({
                        sendCommand() // 두 번째 명령 전송
                    }, 1000)
                } else if (configWriteStep == 1) {
                    // 두 번째 명령 성공 (maxAmmo)
                    Log.d(TAG, "Second command (maxAmmo) successful, config write complete")
                    currentCallback?.onSuccess("Config 설정 완료!")
                    cleanup()
                }
            }
            response.contains("result: fail") -> {
                currentCallback?.onError("Config 설정 실패: $response")
                cleanup()
            }
        }
    }

    @RequiresPermission(Manifest.permission.BLUETOOTH_CONNECT)
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
                currentCallback?.onError("알림 설정 실패: ${e.message}")
                cleanup()
            }
        }
    }

    @RequiresPermission(Manifest.permission.BLUETOOTH_CONNECT)
    private fun sendCommand() {
        val commandToSend = when {
            currentCommand.startsWith("config_write_") -> {
                // config_write_50:30 형태에서 파라미터 추출
                val params = currentCommand.substringAfter("config_write_")
                val parts = params.split(":")
                if (parts.size == 2) {
                    val debounceDelay = parts[0]
                    val maxAmmo = parts[1]
                    
                    // configWriteStep에 따라 명령 결정
                    when (configWriteStep) {
                        0 -> "config set debounceDelay $debounceDelay"  // 첫 번째 명령
                        1 -> "ammo setmax $maxAmmo"  // 두 번째 명령
                        else -> currentCommand
                    }
                } else {
                    currentCommand
                }
            }
            else -> currentCommand
        }
        
        val bytes = commandToSend.toByteArray(Charsets.UTF_8)
        Log.d(TAG, "Sending command: '$commandToSend'")

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
                currentCallback?.onError("명령 전송 실패: ${e.message}")
                cleanup()
            }
        }
    }

    @RequiresPermission(Manifest.permission.BLUETOOTH_CONNECT)
    private fun startResponseTimeout() {
        Log.d(TAG, "Starting response timeout (${RESPONSE_TIMEOUT / 1000}초)")
        
        responseTimeoutRunnable = Runnable {
            Log.e(TAG, "*** RESPONSE TIMEOUT ***")
            handler.post {
                currentCallback?.onError("응답 타임아웃 (${RESPONSE_TIMEOUT / 1000}초)\n명령: $currentCommand")
                cleanup()
            }
        }
        handler.postDelayed(responseTimeoutRunnable!!, RESPONSE_TIMEOUT)
    }

    private fun cancelResponseTimeout() {
        responseTimeoutRunnable?.let {
            Log.d(TAG, "Canceling response timeout")
            handler.removeCallbacks(it)
            responseTimeoutRunnable = null
        }
    }

    @RequiresPermission(Manifest.permission.BLUETOOTH_CONNECT)
    private fun cleanup() {
        Log.d(TAG, "Cleaning up")
        isConnecting = false
        currentCallback = null
        targetCharacteristic = null
        responseBuffer.clear()
        commandSent = false
        currentCommand = ""
        configWriteStep = 0  // config write 단계 초기화

        // 모든 타임아웃 취소
        timeoutRunnable?.let { 
            handler.removeCallbacks(it)
            timeoutRunnable = null
        }
        cancelResponseTimeout()

        try {
            bluetoothGatt?.close()
        } catch (e: Exception) {
            Log.e(TAG, "GATT close failed", e)
        }
        bluetoothGatt = null
    }

    fun isConnecting(): Boolean = isConnecting
}
