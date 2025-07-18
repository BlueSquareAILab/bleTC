package com.example.bsmagapp

import android.Manifest
import android.bluetooth.BluetoothAdapter
import android.bluetooth.BluetoothManager
import android.bluetooth.le.BluetoothLeScanner
import android.bluetooth.le.ScanCallback
import android.bluetooth.le.ScanResult
import android.content.Context
import android.content.Intent
import android.content.pm.PackageManager
import android.os.Build
import android.os.Bundle
import android.os.Handler
import android.os.Looper
import android.widget.Button
import android.widget.Toast
import androidx.activity.result.contract.ActivityResultContracts
import androidx.appcompat.app.AlertDialog
import androidx.appcompat.app.AppCompatActivity
import androidx.core.content.ContextCompat
import androidx.recyclerview.widget.LinearLayoutManager
import androidx.recyclerview.widget.RecyclerView

class MainActivity : AppCompatActivity() {

    private lateinit var bluetoothAdapter: BluetoothAdapter
    private lateinit var bluetoothLeScanner: BluetoothLeScanner
    private lateinit var recyclerView: RecyclerView
    private lateinit var bleDeviceAdapter: BleDeviceAdapter
    private lateinit var scanButton: Button
    private lateinit var stopButton: Button
    private lateinit var bleCommander: BleCommander

    private var isScanning = false
    private val handler = Handler(Looper.getMainLooper())
    private val scanPeriod: Long = 10000 // 10초
    private val devices = mutableListOf<BleDevice>()

    // 권한 요청 결과 처리
    private val requestPermissionLauncher = registerForActivityResult(
        ActivityResultContracts.RequestMultiplePermissions()
    ) { permissions ->
        var allGranted = true
        val deniedPermissions = mutableListOf<String>()

        permissions.entries.forEach { entry ->
            if (!entry.value) {
                allGranted = false
                deniedPermissions.add(entry.key)
            }
        }

        if (allGranted) {
            Toast.makeText(this, "모든 권한이 허용되었습니다", Toast.LENGTH_SHORT).show()
            initializeBluetooth()
        } else {
            Toast.makeText(this, "권한이 거부되었습니다 ${deniedPermissions.joinToString()}", Toast.LENGTH_LONG).show()
            showPermissionDialog()
        }
    }

    // 블루투스 활성화 요청
    private val bluetoothEnableRequest = registerForActivityResult(
        ActivityResultContracts.StartActivityForResult()
    ) { result ->
        if (result.resultCode == RESULT_OK) {
            Toast.makeText(this, "블루투스가 활성화되었습니다.", Toast.LENGTH_SHORT).show()
            initializeBluetooth()
        } else {
            Toast.makeText(this, "블루투스 활성화가 취소됨", Toast.LENGTH_SHORT).show()
        }
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_main)

        initViews()
        checkAndRequestPermissions()
    }

    private fun initViews() {
        recyclerView = findViewById(R.id.recyclerView)
        scanButton = findViewById(R.id.scanButton)
        stopButton = findViewById(R.id.stopButton)

        // BLE Commander 초기화
        bleCommander = BleCommander(this)

        bleDeviceAdapter = BleDeviceAdapter(
            devices = devices,
            onDeviceClick = { device ->
                Toast.makeText(this, "선택된 디바이스: ${device.name}", Toast.LENGTH_SHORT).show()
            },
            onAmmoReset = { device ->
                performAmmoReset(device)
            }
        )

        recyclerView.adapter = bleDeviceAdapter
        recyclerView.layoutManager = LinearLayoutManager(this)

        scanButton.setOnClickListener {
            if (!isScanning) {
                if (hasAllPermissions()) {
                    startScan()
                } else {
                    checkAndRequestPermissions()
                }
            }
        }

        stopButton.setOnClickListener {
            if (isScanning) {
                stopScan()
            }
        }

        updateButtonStates()
    }

    private fun performAmmoReset(device: BleDevice) {
        if (bleCommander.isConnecting()) {
            Toast.makeText(this, "이미 다른 명령이 실행 중입니다", Toast.LENGTH_SHORT).show()
            return
        }

        // 현재 처리 중인 디바이스 저장
        var currentProgress = ""

        bleCommander.executeAmmoReset(device.device, object : BleCommander.BleCommandCallback {
            override fun onSuccess(response: String) {
                runOnUiThread {
                    Toast.makeText(this@MainActivity, "✅ $response", Toast.LENGTH_SHORT).show()

                    // 성공 다이얼로그 표시
//                    showResponseDialog("🎉 Ammo Reset 성공", response)

                    // 버튼 상태를 성공으로 변경 (어댑터에서 자동으로 2초 후 복원)
                    updateButtonStateForDevice(device.address, BleDeviceAdapter.ButtonState.SUCCESS)
                }
            }

            override fun onError(error: String) {
                runOnUiThread {
                    Toast.makeText(this@MainActivity, "❌ $error", Toast.LENGTH_LONG).show()

                    // 실패 다이얼로그 표시
                    showResponseDialog("❌ Ammo Reset 실패", error)

                    // 버튼 상태를 실패로 변경 (어댑터에서 자동으로 3초 후 복원)
                    updateButtonStateForDevice(device.address, BleDeviceAdapter.ButtonState.ERROR)
                }
            }

            override fun onProgress(message: String) {
                runOnUiThread {
                    currentProgress = message
                    // 간단한 토스트로 진행 상황 표시
                    if (message.contains("연결 중") || message.contains("명령 전송") || message.contains("완료")) {
                        Toast.makeText(this@MainActivity, message, Toast.LENGTH_SHORT).show()
                    }
                }
            }
        })
    }

    private fun updateButtonStateForDevice(deviceAddress: String, state: BleDeviceAdapter.ButtonState) {
        // RecyclerView에서 해당 디바이스의 ViewHolder를 찾아서 버튼 상태 업데이트
        val layoutManager = recyclerView.layoutManager as? LinearLayoutManager
        if (layoutManager != null) {
            val position = devices.indexOfFirst { it.address == deviceAddress }
            if (position != -1) {
                val viewHolder = recyclerView.findViewHolderForAdapterPosition(position) as? BleDeviceAdapter.BleDeviceViewHolder
                viewHolder?.setButtonState(state)
            }
        }
    }


    private fun showResponseDialog(title: String, message: String) {
        AlertDialog.Builder(this)
            .setTitle(title)
            .setMessage(message)
            .setPositiveButton("확인", null)
            .show()
    }

    private fun checkAndRequestPermissions() {
        val permissions = mutableListOf<String>()

        // 위치 권한 (BLE 스캔에 필요)
        if (ContextCompat.checkSelfPermission(this, Manifest.permission.ACCESS_FINE_LOCATION)
            != PackageManager.PERMISSION_GRANTED) {
            permissions.add(Manifest.permission.ACCESS_FINE_LOCATION)
        }

        // Android 12 이상에서 필요한 블루투스 권한들
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            if (ContextCompat.checkSelfPermission(this, Manifest.permission.BLUETOOTH_SCAN)
                != PackageManager.PERMISSION_GRANTED) {
                permissions.add(Manifest.permission.BLUETOOTH_SCAN)
            }
            if (ContextCompat.checkSelfPermission(this, Manifest.permission.BLUETOOTH_CONNECT)
                != PackageManager.PERMISSION_GRANTED) {
                permissions.add(Manifest.permission.BLUETOOTH_CONNECT)
            }
        }

        if (permissions.isNotEmpty()) {
            Toast.makeText(this, "BLE 스캔을 위해 권한이 필요합니다", Toast.LENGTH_SHORT).show()
            requestPermissionLauncher.launch(permissions.toTypedArray())
        } else {
            initializeBluetooth()
        }
    }

    private fun hasAllPermissions(): Boolean {
        val locationPermission = ContextCompat.checkSelfPermission(this, Manifest.permission.ACCESS_FINE_LOCATION) == PackageManager.PERMISSION_GRANTED

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            val bluetoothScanPermission = ContextCompat.checkSelfPermission(this, Manifest.permission.BLUETOOTH_SCAN) == PackageManager.PERMISSION_GRANTED
            val bluetoothConnectPermission = ContextCompat.checkSelfPermission(this, Manifest.permission.BLUETOOTH_CONNECT) == PackageManager.PERMISSION_GRANTED
            return locationPermission && bluetoothScanPermission && bluetoothConnectPermission
        }

        return locationPermission
    }

    private fun showPermissionDialog() {
        AlertDialog.Builder(this)
            .setTitle("권한 필요")
            .setMessage("BLE 디바이스 스캔을 위해서는 위치 및 블루투스 권한이 필요합니다\n설정에서 권한을 허용해주세요.")
            .setPositiveButton("설정으로 이동") { _, _ ->
                val intent = Intent(android.provider.Settings.ACTION_APPLICATION_DETAILS_SETTINGS)
                intent.data = android.net.Uri.parse("package:$packageName")
                startActivity(intent)
            }
            .setNegativeButton("취소", null)
            .show()
    }

    private fun initializeBluetooth() {
        val bluetoothManager = getSystemService(Context.BLUETOOTH_SERVICE) as BluetoothManager
        bluetoothAdapter = bluetoothManager.adapter

        if (bluetoothAdapter == null) {
            Toast.makeText(this, "이 디바이스는 블루투스를 지원하지 않습니다.", Toast.LENGTH_SHORT).show()
            return
        }

        if (!bluetoothAdapter.isEnabled) {
            val enableBtIntent = Intent(BluetoothAdapter.ACTION_REQUEST_ENABLE)
            bluetoothEnableRequest.launch(enableBtIntent)
        } else {
            bluetoothLeScanner = bluetoothAdapter.bluetoothLeScanner
            Toast.makeText(this, "블루투스가 준비되었습니다. 스캔을 시작하세요", Toast.LENGTH_SHORT).show()
        }
    }

    private fun startScan() {
        if (!hasAllPermissions()) {
            Toast.makeText(this, "권한이 없습니다. 권한을 확인해주세요.", Toast.LENGTH_SHORT).show()
            checkAndRequestPermissions()
            return
        }

        if (!::bluetoothLeScanner.isInitialized) {
            Toast.makeText(this, "블루투스를 초기화하는 중입니다.", Toast.LENGTH_SHORT).show()
            initializeBluetooth()
            return
        }

        bleDeviceAdapter.clearDevices()
        isScanning = true
        updateButtonStates()

        try {
            bluetoothLeScanner.startScan(scanCallback)
            Toast.makeText(this, "BSQTC 디바이스 스캔을 시작합니다", Toast.LENGTH_SHORT).show()

            // 10초 후 자동으로 스캔 중지
            handler.postDelayed({
                stopScan()
            }, scanPeriod)
        } catch (e: SecurityException) {
            Toast.makeText(this, "스캔 권한이 없습니다: ${e.message}", Toast.LENGTH_LONG).show()
            isScanning = false
            updateButtonStates()
        } catch (e: Exception) {
            Toast.makeText(this, "스캔 시작 실패: ${e.message}", Toast.LENGTH_LONG).show()
            isScanning = false
            updateButtonStates()
        }
    }

    private fun stopScan() {
        if (!isScanning) return

        isScanning = false
        updateButtonStates()

        try {
            if (::bluetoothLeScanner.isInitialized) {
                bluetoothLeScanner.stopScan(scanCallback)
                Toast.makeText(this, "BLE 스캔이 중지되었습니다", Toast.LENGTH_SHORT).show()
            }
        } catch (e: SecurityException) {
            Toast.makeText(this, "스캔 중지 권한이 없습니다: ${e.message}", Toast.LENGTH_LONG).show()
        } catch (e: Exception) {
            Toast.makeText(this, "스캔 중지 실패: ${e.message}", Toast.LENGTH_LONG).show()
        }
    }

    private fun updateButtonStates() {
        scanButton.isEnabled = !isScanning
        stopButton.isEnabled = isScanning

        scanButton.text = if (isScanning) "스캔 중.." else "스캔 시작"
    }

    private val scanCallback = object : ScanCallback() {
        override fun onScanResult(callbackType: Int, result: ScanResult) {
            super.onScanResult(callbackType, result)

            // BSQTC로 시작하는 디바이스만 필터링
            val deviceName = getDeviceName(result)
            if (deviceName != null && deviceName.startsWith("BSQTC", ignoreCase = true)) {
                val device = BleDevice(
                    device = result.device,
                    rssi = result.rssi,
                    scanRecord = result.scanRecord?.bytes
                )

                runOnUiThread {
                    bleDeviceAdapter.addDevice(device)
                }
            }
        }

        // 디바이스 이름을 가져오는 헬퍼 함수 (ScanRecord에서도 확인)
        private fun getDeviceName(result: ScanResult): String? {
            // 1. BluetoothDevice의 이름 확인
            val deviceName = result.device.name
            if (!deviceName.isNullOrEmpty()) {
                return deviceName
            }

            // 2. ScanRecord의 이름 확인 (Local Name)
            val scanRecord = result.scanRecord
            if (scanRecord != null) {
                val localName = scanRecord.deviceName
                if (!localName.isNullOrEmpty()) {
                    return localName
                }
            }

            return null
        }

        override fun onScanFailed(errorCode: Int) {
            super.onScanFailed(errorCode)
            runOnUiThread {
                val errorMessage = when (errorCode) {
                    SCAN_FAILED_ALREADY_STARTED -> "스캔이 이미 시작되었습니다"
                    SCAN_FAILED_APPLICATION_REGISTRATION_FAILED -> "앱 등록 실패"
                    SCAN_FAILED_FEATURE_UNSUPPORTED -> "BLE 기능이 지원되지 않습니다"
                    SCAN_FAILED_INTERNAL_ERROR -> "내부 오류"
                    else -> "알 수 없는 오류: $errorCode"
                }
                Toast.makeText(this@MainActivity, "스캔 실패: $errorMessage", Toast.LENGTH_LONG).show()
                isScanning = false
                updateButtonStates()
            }
        }
    }

    override fun onDestroy() {
        super.onDestroy()
        if (isScanning) {
            stopScan()
        }
    }


}