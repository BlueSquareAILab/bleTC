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
import android.util.Log
import android.widget.Button
import android.widget.EditText
import android.widget.TextView
import android.widget.Toast
import androidx.activity.result.contract.ActivityResultContracts
import androidx.annotation.RequiresPermission
import androidx.appcompat.app.AlertDialog
import androidx.appcompat.app.AppCompatActivity
import androidx.core.content.ContextCompat
import androidx.recyclerview.widget.LinearLayoutManager
import androidx.recyclerview.widget.RecyclerView

class MainActivity : AppCompatActivity() {

    companion object {
        private const val TAG = "MainActivity"
    }

    private lateinit var bluetoothAdapter: BluetoothAdapter
    private lateinit var bluetoothLeScanner: BluetoothLeScanner
    private lateinit var recyclerView: RecyclerView
    private lateinit var bleDeviceAdapter: BleDeviceAdapter
    private lateinit var scanButton: Button
    private lateinit var stopButton: Button
    private lateinit var sequentialButton: Button
    private lateinit var scanStatusText: TextView
    private lateinit var bleCommander: BleCommander
    private lateinit var edtDebounceDelay: EditText
    private lateinit var edtAmmo: EditText

    private var isScanning = false
    private var isSequentialProcessing = false
    private val handler = Handler(Looper.getMainLooper())
    private val scanPeriod: Long = 10000 // 10초
    private val devices = mutableListOf<BleDevice>()

    // 스캔 타이머 관련 변수들
    private var scanStartTime = 0L
    private var scanCountdownRunnable: Runnable? = null

    // 순회 처리 관련 변수들
    private var currentProcessingIndex = 0

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
        scanStatusText = findViewById(R.id.scanStatusText)
        sequentialButton = findViewById(R.id.sequentialButton)
        edtDebounceDelay = findViewById(R.id.edtDebounceDelay)
        edtAmmo = findViewById(R.id.edtAmmo)
        // BLE Commander 초기화
        bleCommander = BleCommander(this)

        bleDeviceAdapter = BleDeviceAdapter(
            devices = devices,
            onDeviceClick = { device ->
                Toast.makeText(this, "선택된 디바이스: ${device.name}", Toast.LENGTH_SHORT).show()
            },
            onAmmoReset = { device ->
                if (!isScanning && !isSequentialProcessing) {
                    performAmmoReset(device)
                }
            },
            onPulseTest = { device ->  // pulse test 콜백 추가
                if (!isScanning && !isSequentialProcessing) {
                    performPulseTest(device)
                }
            },
            onConfigDump = { device ->  // config dump 콜백 추가
                if (!isScanning && !isSequentialProcessing) {
                    performConfigDump(device)
                }
            },
            onConfigWrite = { device ->  // config write 콜백 추가
                if (!isScanning && !isSequentialProcessing) {
                    performConfigWrite(device)
                }
            },
            isButtonsEnabled = { !isScanning && !isSequentialProcessing }
        )

        recyclerView.adapter = bleDeviceAdapter
        recyclerView.layoutManager = LinearLayoutManager(this)

        scanButton.setOnClickListener {
            if (!isScanning && !isSequentialProcessing) {
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

        // 순회 처리 버튼
        sequentialButton.setOnClickListener {
            if (!isScanning && !isSequentialProcessing && devices.isNotEmpty()) {
                startSequentialProcessing()
            } else if (devices.isEmpty()) {
                Toast.makeText(this, "처리할 디바이스가 없습니다. 먼저 스캔해주세요.", Toast.LENGTH_SHORT).show()
            }
        }

        updateButtonStates()
    }

    @RequiresPermission(Manifest.permission.BLUETOOTH_CONNECT)
    private fun startSequentialProcessing() {
        if (devices.isEmpty()) {
            Toast.makeText(this, "처리할 디바이스가 없습니다", Toast.LENGTH_SHORT).show()
            return
        }

        isSequentialProcessing = true
        currentProcessingIndex = 0
        updateButtonStates()
        updateScanStatus("순회 처리 시작 - ${devices.size}개 디바이스")

        processNextDevice()
    }

    @RequiresPermission(Manifest.permission.BLUETOOTH_CONNECT)
    private fun processNextDevice() {
        if (currentProcessingIndex >= devices.size) {
            // 모든 디바이스 처리 완료
            isSequentialProcessing = false
            updateButtonStates()
            updateScanStatus("순회 처리 완료 - ${devices.size}개 디바이스 처리됨")
            Toast.makeText(this, "🎉 모든 디바이스 처리가 완료되었습니다!", Toast.LENGTH_LONG).show()
            return
        }

        val device = devices[currentProcessingIndex]
        val deviceNumber = currentProcessingIndex + 1
        val totalDevices = devices.size

        updateScanStatus("처리 중... ($deviceNumber/$totalDevices) - ${device.name}")

        // 현재 디바이스의 버튼 상태를 처리 중으로 변경
        updateButtonStateForDevice(device.address, BleDeviceAdapter.ButtonState.PROCESSING)

        // BleCommander로 명령 전송
        bleCommander.executeAmmoReset(device.device, object : BleCommander.BleCommandCallback {
            override fun onSuccess(response: String) {
                runOnUiThread {
                    Toast.makeText(this@MainActivity, "✅ ${device.name}: $response", Toast.LENGTH_SHORT).show()
                    updateButtonStateForDevice(device.address, BleDeviceAdapter.ButtonState.SUCCESS)

                    // 다음 디바이스로 진행
                    currentProcessingIndex++
                    handler.postDelayed({
                        processNextDevice()
                    }, 1500) // 1.5초 대기 후 다음 디바이스
                }
            }

            override fun onError(error: String) {
                runOnUiThread {
                    Toast.makeText(this@MainActivity, "❌ ${device.name}: $error", Toast.LENGTH_LONG).show()
                    updateButtonStateForDevice(device.address, BleDeviceAdapter.ButtonState.ERROR)

                    // 실패해도 다음 디바이스로 진행
                    currentProcessingIndex++
                    handler.postDelayed({
                        processNextDevice()
                    }, 2000) // 2초 대기 후 다음 디바이스
                }
            }

            override fun onProgress(message: String) {
                runOnUiThread {
                    val deviceNumber = currentProcessingIndex + 1
                    val totalDevices = devices.size
                    updateScanStatus("처리 중... ($deviceNumber/$totalDevices) - ${device.name}: $message")
                }
            }
        })
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

        // 기존 스캔 타이머 취소
        handler.removeCallbacksAndMessages(null)
        
        // 디바이스 리스트 클리어
        devices.clear()
        bleDeviceAdapter.clearDevices()
        
        isScanning = true
        scanStartTime = System.currentTimeMillis()
        updateButtonStates()

        try {
            bluetoothLeScanner.startScan(scanCallback)
            Toast.makeText(this, "BSQTC 디바이스 스캔을 시작합니다 (10초)", Toast.LENGTH_SHORT).show()

            // 카운트다운 시작
            startScanCountdown()

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
        updateButtonStates()  // 첫 번째 호출

        // 카운트다운 중지
        scanCountdownRunnable?.let {
            handler.removeCallbacks(it)
            scanCountdownRunnable = null
        }

        try {
            if (::bluetoothLeScanner.isInitialized) {
                bluetoothLeScanner.stopScan(scanCallback)
                val deviceCount = devices.size
                updateScanStatus("스캔 완료 - ${deviceCount}개 디바이스 발견")
                Toast.makeText(this, "BLE 스캔이 완료되었습니다 (${deviceCount}개 발견)", Toast.LENGTH_SHORT).show()
                updateButtonStates()  // 두 번째 호출 - 스캔 완료 후 버튼 텍스트 업데이트
            }
        } catch (e: SecurityException) {
            Toast.makeText(this, "스캔 중지 권한이 없습니다: ${e.message}", Toast.LENGTH_LONG).show()
        } catch (e: Exception) {
            Toast.makeText(this, "스캔 중지 실패: ${e.message}", Toast.LENGTH_LONG).show()
        }
    }

    private fun updateButtonStates() {
        // 스캔 버튼들
        scanButton.isEnabled = !isScanning && !isSequentialProcessing
        stopButton.isEnabled = isScanning

        // 순회 처리 버튼
        sequentialButton.isEnabled = !isScanning && !isSequentialProcessing && devices.isNotEmpty()

        // 버튼 텍스트 업데이트
        if (isScanning) {
            scanButton.text = "스캔 중..."
            stopButton.text = "중지"
        } else {
            scanButton.text = "스캔 시작"
            stopButton.text = "스캔 중지"
            updateScanStatus("스캔 대기 중")
        }

        if (isSequentialProcessing) {
            sequentialButton.text = "처리 중..."
        } else {
            sequentialButton.text = "전체 리셋 (${devices.size}개)"
        }

        // RecyclerView 어댑터에 상태 변경 알림
        bleDeviceAdapter.notifyDataSetChanged()
    }

    private fun startScanCountdown() {
        scanCountdownRunnable = object : Runnable {
            override fun run() {
                if (isScanning) {
                    val elapsed = System.currentTimeMillis() - scanStartTime
                    val remaining = (scanPeriod - elapsed) / 1000

                    if (remaining > 0) {
                        updateScanStatus("스캔 중... ${remaining}초 남음 (${devices.size}개 발견)")
                        // 카운트다운 중에도 버튼 텍스트 업데이트 (디바이스 수 변경 반영)
                        updateButtonStates()
                        handler.postDelayed(this, 1000)
                    } else {
                        updateScanStatus("스캔 완료")
                    }
                }
            }
        }
        handler.post(scanCountdownRunnable!!)
    }

    private fun updateScanStatus(message: String) {
        scanStatusText.text = message
    }

    private fun updateButtonStateForDevice(deviceAddress: String, state: BleDeviceAdapter.ButtonState) {
        val layoutManager = recyclerView.layoutManager as? LinearLayoutManager
        if (layoutManager != null) {
            val position = devices.indexOfFirst { it.address == deviceAddress }
            if (position != -1) {
                val viewHolder = recyclerView.findViewHolderForAdapterPosition(position) as? BleDeviceAdapter.BleDeviceViewHolder
                viewHolder?.setButtonState(state)
            }
        }
    }

    private fun updatePulseButtonStateForDevice(deviceAddress: String, state: BleDeviceAdapter.ButtonState) {
        val layoutManager = recyclerView.layoutManager as? LinearLayoutManager
        if (layoutManager != null) {
            val position = devices.indexOfFirst { it.address == deviceAddress }
            if (position != -1) {
                val viewHolder = recyclerView.findViewHolderForAdapterPosition(position) as? BleDeviceAdapter.BleDeviceViewHolder
                viewHolder?.setPulseButtonState(state)
            }
        }
    }

    private fun updateConfigButtonStateForDevice(deviceAddress: String, state: BleDeviceAdapter.ButtonState) {
        val layoutManager = recyclerView.layoutManager as? LinearLayoutManager
        if (layoutManager != null) {
            val position = devices.indexOfFirst { it.address == deviceAddress }
            if (position != -1) {
                val viewHolder = recyclerView.findViewHolderForAdapterPosition(position) as? BleDeviceAdapter.BleDeviceViewHolder
                viewHolder?.setConfigButtonState(state)
            }
        }
    }

    private fun updateWriteButtonStateForDevice(deviceAddress: String, state: BleDeviceAdapter.ButtonState) {
        val layoutManager = recyclerView.layoutManager as? LinearLayoutManager
        if (layoutManager != null) {
            val position = devices.indexOfFirst { it.address == deviceAddress }
            if (position != -1) {
                val viewHolder = recyclerView.findViewHolderForAdapterPosition(position) as? BleDeviceAdapter.BleDeviceViewHolder
                viewHolder?.setWriteButtonState(state)
            }
        }
    }

    @RequiresPermission(Manifest.permission.BLUETOOTH_CONNECT)
    private fun performPulseTest(device: BleDevice) {
        if (bleCommander.isConnecting()) {
            Toast.makeText(this, "이미 다른 명령이 실행 중입니다", Toast.LENGTH_SHORT).show()
            return
        }

        // 현재 처리 중인 디바이스 저장
        updatePulseButtonStateForDevice(device.address, BleDeviceAdapter.ButtonState.PROCESSING)

        bleCommander.executePulseTest(device.device, object : BleCommander.BleCommandCallback {
            override fun onSuccess(response: String) {
                runOnUiThread {
                    Toast.makeText(this@MainActivity, "✅ Pulse 테스트 성공: $response", Toast.LENGTH_SHORT).show()
                    updatePulseButtonStateForDevice(device.address, BleDeviceAdapter.ButtonState.SUCCESS)
                }
            }

            override fun onError(error: String) {
                runOnUiThread {
                    Toast.makeText(this@MainActivity, "❌ Pulse 테스트 실패: $error", Toast.LENGTH_LONG).show()
                    showResponseDialog("❌ Pulse Test 실패", error)
                    updatePulseButtonStateForDevice(device.address, BleDeviceAdapter.ButtonState.ERROR)
                }
            }

            override fun onProgress(message: String) {
                runOnUiThread {
                    if (message.contains("연결 중") || message.contains("명령 전송") || message.contains("완료")) {
                        Toast.makeText(this@MainActivity, "Pulse: $message", Toast.LENGTH_SHORT).show()
                    }
                }
            }
        })
    }

    @RequiresPermission(Manifest.permission.BLUETOOTH_CONNECT)
    private fun performConfigDump(device: BleDevice) {
        if (bleCommander.isConnecting()) {
            Toast.makeText(this, "이미 다른 명령이 실행 중입니다", Toast.LENGTH_SHORT).show()
            return
        }

        // 현재 처리 중인 디바이스 저장
        updateConfigButtonStateForDevice(device.address, BleDeviceAdapter.ButtonState.PROCESSING)

        bleCommander.executeConfigDump(device.device, object : BleCommander.BleCommandCallback {
            override fun onSuccess(response: String) {
                runOnUiThread {
                    Toast.makeText(this@MainActivity, "✅ Config 조회 성공", Toast.LENGTH_SHORT).show()
                    updateConfigButtonStateForDevice(device.address, BleDeviceAdapter.ButtonState.SUCCESS)
                    
                    // Config dump 응답 파싱하여 TextView에 표시
                    parseAndDisplayConfig(response)
                }
            }

            override fun onError(error: String) {
                runOnUiThread {
                    Toast.makeText(this@MainActivity, "❌ Config 조회 실패: $error", Toast.LENGTH_LONG).show()
                    showResponseDialog("❌ Config Dump 실패", error)
                    updateConfigButtonStateForDevice(device.address, BleDeviceAdapter.ButtonState.ERROR)
                }
            }

            override fun onProgress(message: String) {
                runOnUiThread {
                    if (message.contains("연결 중") || message.contains("명령 전송") || message.contains("완료")) {
                        Toast.makeText(this@MainActivity, "Config: $message", Toast.LENGTH_SHORT).show()
                    }
                }
            }
        })
    }

    @RequiresPermission(Manifest.permission.BLUETOOTH_CONNECT)
    private fun performConfigWrite(device: BleDevice) {
        if (bleCommander.isConnecting()) {
            Toast.makeText(this, "이미 다른 명령이 실행 중입니다", Toast.LENGTH_SHORT).show()
            return
        }

        // 입력값 검증
        val debounceDelayStr = edtDebounceDelay.text.toString().trim()
        val ammoStr = edtAmmo.text.toString().trim()

        if (debounceDelayStr.isEmpty() || ammoStr.isEmpty()) {
            Toast.makeText(this, "모든 값을 입력해주세요 (debounceDelay, maxAmmo)", Toast.LENGTH_LONG).show()
            return
        }

        val debounceDelay = try {
            debounceDelayStr.toInt()
        } catch (e: NumberFormatException) {
            Toast.makeText(this, "debounceDelay는 숫자로 입력해주세요", Toast.LENGTH_SHORT).show()
            return
        }

        val maxAmmo = try {
            ammoStr.toInt()
        } catch (e: NumberFormatException) {
            Toast.makeText(this, "maxAmmo는 숫자로 입력해주세요", Toast.LENGTH_SHORT).show()
            return
        }

        // 범위 검증
        if (debounceDelay < 0 || debounceDelay > 1000) {
            Toast.makeText(this, "debounceDelay는 0~1000 범위로 입력해주세요", Toast.LENGTH_SHORT).show()
            return
        }

        if (maxAmmo < 1 || maxAmmo > 999) {
            Toast.makeText(this, "maxAmmo는 1~999 범위로 입력해주세요", Toast.LENGTH_SHORT).show()
            return
        }

        // 현재 처리 중인 디바이스 저장
        updateWriteButtonStateForDevice(device.address, BleDeviceAdapter.ButtonState.PROCESSING)

        bleCommander.executeConfigWrite(device.device, debounceDelay, maxAmmo, object : BleCommander.BleCommandCallback {
            override fun onSuccess(response: String) {
                runOnUiThread {
                    Toast.makeText(this@MainActivity, "✅ Config 설정 성공: $response", Toast.LENGTH_SHORT).show()
                    updateWriteButtonStateForDevice(device.address, BleDeviceAdapter.ButtonState.SUCCESS)
                }
            }

            override fun onError(error: String) {
                runOnUiThread {
                    Toast.makeText(this@MainActivity, "❌ Config 설정 실패: $error", Toast.LENGTH_LONG).show()
                    showResponseDialog("❌ Config Write 실패", error)
                    updateWriteButtonStateForDevice(device.address, BleDeviceAdapter.ButtonState.ERROR)
                }
            }

            override fun onProgress(message: String) {
                runOnUiThread {
                    if (message.contains("연결 중") || message.contains("명령 전송") || message.contains("완료")) {
                        Toast.makeText(this@MainActivity, "Config Write: $message", Toast.LENGTH_SHORT).show()
                    }
                }
            }
        })
    }

    private fun performAmmoReset(device: BleDevice) {
        if (bleCommander.isConnecting()) {
            Toast.makeText(this, "이미 다른 명령이 실행 중입니다", Toast.LENGTH_SHORT).show()
            return
        }

        // 현재 처리 중인 디바이스 저장
        updateButtonStateForDevice(device.address, BleDeviceAdapter.ButtonState.PROCESSING)

        bleCommander.executeAmmoReset(device.device, object : BleCommander.BleCommandCallback {
            override fun onSuccess(response: String) {
                runOnUiThread {
                    Toast.makeText(this@MainActivity, "✅ $response", Toast.LENGTH_SHORT).show()
                    updateButtonStateForDevice(device.address, BleDeviceAdapter.ButtonState.SUCCESS)
                }
            }

            override fun onError(error: String) {
                runOnUiThread {
                    Toast.makeText(this@MainActivity, "❌ $error", Toast.LENGTH_LONG).show()
                    showResponseDialog("❌ Ammo Reset 실패", error)
                    updateButtonStateForDevice(device.address, BleDeviceAdapter.ButtonState.ERROR)
                }
            }

            override fun onProgress(message: String) {
                runOnUiThread {
                    if (message.contains("연결 중") || message.contains("명령 전송") || message.contains("완료")) {
                        Toast.makeText(this@MainActivity, message, Toast.LENGTH_SHORT).show()
                    }
                }
            }
        })
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
                    scanRecord = result.scanRecord?.bytes,
                    originalName = deviceName  // 스캔 시점의 이름 저장
                )

                runOnUiThread {
                    bleDeviceAdapter.addDevice(device)
                    Log.d(TAG, "Adding device to adapter: ${device.name} - ${device.address}")
                    Log.d(TAG, "Current device count: ${devices.size}")
                    
                    // 버튼 상태 업데이트 (새로 발견된 디바이스는 스캔 상태에 따라 비활성화)
                    updateButtonStates()

                    // 실시간으로 발견된 디바이스 수 업데이트
                    if (isScanning) {
                        val elapsed = System.currentTimeMillis() - scanStartTime
                        val remaining = (scanPeriod - elapsed) / 1000
                        if (remaining > 0) {
                            updateScanStatus("스캔 중... ${remaining}초 남음 (${devices.size}개 발견)")
                        }
                    } else {
                        // 스캔이 끝난 후에도 버튼 텍스트 업데이트
                        updateButtonStates()
                    }
                }
            }
        }

        // 디바이스 이름을 가져오는 헬퍼 함수 (ScanRecord에서도 확인)
        @RequiresPermission(Manifest.permission.BLUETOOTH_CONNECT)
        private fun getDeviceName(result: ScanResult): String? {
            try {
                // 1. BluetoothDevice의 이름 확인
                val deviceName = result.device.name
                if (!deviceName.isNullOrEmpty()) {
                    return deviceName
                }
            } catch (e: SecurityException) {
                Log.w(TAG, "BLUETOOTH_CONNECT permission required for device.name")
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
                updateScanStatus("스캔 실패: $errorMessage")
                isScanning = false
                updateButtonStates()

                // 실패 시 카운트다운도 중지
                scanCountdownRunnable?.let {
                    handler.removeCallbacks(it)
                    scanCountdownRunnable = null
                }
            }
        }
    }

    @RequiresPermission(Manifest.permission.BLUETOOTH_CONNECT)
    private fun getDeviceName(result: ScanResult): String? {
        try {
            val deviceName = result.device.name
            if (!deviceName.isNullOrEmpty()) {
                return deviceName
            }
        } catch (e: SecurityException) {
            Log.w(TAG, "BLUETOOTH_CONNECT permission required for device.name")
        }

        val scanRecord = result.scanRecord
        if (scanRecord != null) {
            val localName = scanRecord.deviceName
            if (!localName.isNullOrEmpty()) {
                return localName
            }
        }

        return null
    }

    private fun showResponseDialog(title: String, message: String) {
        AlertDialog.Builder(this)
            .setTitle(title)
            .setMessage(message)
            .setPositiveButton("확인", null)
            .show()
    }

    private fun parseAndDisplayConfig(response: String) {
        try {
            // 응답에서 debounceDelay와 maxAmmo 값 추출
            var debounceDelay = "-"
            var currentAmmo = "_"
            var maxAmmo = "_"
            
            // debounceDelay 추출 (debounceDelay: 값 또는 debounceDelay=값)
            val debouncePattern = Regex("debounceDelay[:\\s=]+(\\d+)")
            debouncePattern.find(response)?.let {
                debounceDelay = it.groupValues[1]
            }
            
            // maxAmmo 추출 (maxAmmo: 값, maxAmmo=값, maxAmmoCount=값)
            val maxAmmoPattern = Regex("(?:maxAmmo|maxAmmoCount)[:\\s=]+(\\d+)")
            maxAmmoPattern.find(response)?.let {
                maxAmmo = it.groupValues[1]
            }
            
            // currentAmmo 추출 (currentAmmo: 값 또는 currentAmmo=값)
            val currentAmmoPattern = Regex("currentAmmo[:\\s=]+(\\d+)")
            currentAmmoPattern.find(response)?.let {
                currentAmmo = it.groupValues[1]
            }
            
            // ammo: current/max 형태도 확인
            val ammoPattern = Regex("ammo:\\s*(\\d+)/(\\d+)")
            ammoPattern.find(response)?.let {
                if (currentAmmo == "_") currentAmmo = it.groupValues[1]
                if (maxAmmo == "_") maxAmmo = it.groupValues[2]
            }
            
            // TextView에 표시
//            configText.text = "debounceDelay: ${debounceDelay}ms, ammo: $currentAmmo/$maxAmmo"
            
            // 값들을 EditText에도 채워줌 (선택사항)
            if (debounceDelay != "-") {
                edtDebounceDelay.setText(debounceDelay)
            }
            if (maxAmmo != "_") {
                edtAmmo.setText(maxAmmo)
            }
            
        } catch (e: Exception) {
            Log.e(TAG, "Config parsing error", e)
//            configText.text = "debounceDelay: -ms, ammo : _/_"
        }
    }
    
    override fun onDestroy() {
        super.onDestroy()
        if (isScanning) {
            stopScan()
        }
        // 핸들러 정리
        scanCountdownRunnable?.let {
            handler.removeCallbacks(it)
        }
    }
}