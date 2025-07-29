package com.example.bsmagapp

import android.util.Log
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.Button
import android.widget.TextView
import androidx.recyclerview.widget.RecyclerView

class BleDeviceAdapter(
    private val devices: MutableList<BleDevice>,
    private val onDeviceClick: (BleDevice) -> Unit,
    private val onAmmoReset: (BleDevice) -> Unit,
    private val onPulseTest: (BleDevice) -> Unit,  // pulse test 콜백 추가
    private val onConfigDump: (BleDevice) -> Unit,  // config dump 콜백 추가
    private val onConfigWrite: (BleDevice) -> Unit,  // config write 콜백 추가
    private val isButtonsEnabled: () -> Boolean
) : RecyclerView.Adapter<BleDeviceAdapter.BleDeviceViewHolder>() {

    inner class BleDeviceViewHolder(itemView: View) : RecyclerView.ViewHolder(itemView) {
        val deviceName: TextView = itemView.findViewById(R.id.deviceName)
        val deviceAddress: TextView = itemView.findViewById(R.id.deviceAddress)
        val deviceRssi: TextView = itemView.findViewById(R.id.deviceRssi)
        val resetAmmoButton: Button = itemView.findViewById(R.id.resetAmmoButton)
        val pulseTestButton: Button = itemView.findViewById(R.id.pulseTestButton)  // pulse test 버튼
        val configDumpButton: Button = itemView.findViewById(R.id.configDumpButton)  // config dump 버튼
        val configSetButton: Button = itemView.findViewById(R.id.configSetButton)  // config write 버튼

        private var currentDevice: BleDevice? = null
        private var ammoButtonState: ButtonState = ButtonState.READY
        private var pulseButtonState: ButtonState = ButtonState.READY
        private var configButtonState: ButtonState = ButtonState.READY
        private var writeButtonState: ButtonState = ButtonState.READY

        init {
            itemView.setOnClickListener {
                val position = adapterPosition
                if (position != RecyclerView.NO_POSITION && isButtonsEnabled()) {
                    onDeviceClick(devices[position])
                }
            }

            resetAmmoButton.setOnClickListener {
                val position = adapterPosition
                if (position != RecyclerView.NO_POSITION && isButtonsEnabled()) {
                    val device = devices[position]
                    currentDevice = device

                    // ammo reset 버튼 상태를 처리 중으로 변경
                    setAmmoButtonState(ButtonState.PROCESSING)

                    onAmmoReset(device)
                }
            }

            pulseTestButton.setOnClickListener {
                val position = adapterPosition
                if (position != RecyclerView.NO_POSITION && isButtonsEnabled()) {
                    val device = devices[position]
                    currentDevice = device

                    // pulse test 버튼 상태를 처리 중으로 변경
                    setPulseButtonState(ButtonState.PROCESSING)

                    onPulseTest(device)
                }
            }

            configDumpButton.setOnClickListener {
                val position = adapterPosition
                if (position != RecyclerView.NO_POSITION && isButtonsEnabled()) {
                    val device = devices[position]
                    currentDevice = device

                    // config dump 버튼 상태를 처리 중으로 변경
                    setConfigButtonState(ButtonState.PROCESSING)

                    onConfigDump(device)
                }
            }

            configSetButton.setOnClickListener {
                val position = adapterPosition
                if (position != RecyclerView.NO_POSITION && isButtonsEnabled()) {
                    val device = devices[position]
                    currentDevice = device

                    // config write 버튼 상태를 처리 중으로 변경
                    setWriteButtonState(ButtonState.PROCESSING)

                    onConfigWrite(device)
                }
            }
        }

        fun setAmmoButtonState(state: ButtonState) {
            ammoButtonState = state
            updateAmmoButtonAppearance()
        }

        fun setPulseButtonState(state: ButtonState) {
            pulseButtonState = state
            updatePulseButtonAppearance()
        }

        fun setConfigButtonState(state: ButtonState) {
            configButtonState = state
            updateConfigButtonAppearance()
        }

        fun setWriteButtonState(state: ButtonState) {
            writeButtonState = state
            updateWriteButtonAppearance()
        }

        // 기존 setButtonState 메서드는 ammo 버튼용으로 유지
        fun setButtonState(state: ButtonState) {
            setAmmoButtonState(state)
        }

        private fun updateAmmoButtonAppearance() {
            val globalEnabled = isButtonsEnabled()
            
            when (ammoButtonState) {
                ButtonState.READY -> {
                    resetAmmoButton.isEnabled = globalEnabled
                    resetAmmoButton.text = "Ammo Reset"
                    resetAmmoButton.setBackgroundColor(
                        if (globalEnabled) 0xFFFF5722.toInt() else 0xFFBDBDBD.toInt()
                    )
                    resetAmmoButton.alpha = if (globalEnabled) 1.0f else 0.6f
                }
                ButtonState.PROCESSING -> {
                    resetAmmoButton.isEnabled = false
                    resetAmmoButton.text = "처리 중..."
                    resetAmmoButton.setBackgroundColor(0xFF9E9E9E.toInt())
                    resetAmmoButton.alpha = 0.8f
                }
                ButtonState.SUCCESS -> {
                    resetAmmoButton.isEnabled = globalEnabled
                    resetAmmoButton.text = "성공!"
                    resetAmmoButton.setBackgroundColor(0xFF4CAF50.toInt())
                    resetAmmoButton.alpha = 1.0f

                    resetAmmoButton.postDelayed({
                        if (ammoButtonState == ButtonState.SUCCESS) {
                            setAmmoButtonState(ButtonState.READY)
                        }
                    }, 2000)
                }
                ButtonState.ERROR -> {
                    resetAmmoButton.isEnabled = globalEnabled
                    resetAmmoButton.text = "실패"
                    resetAmmoButton.setBackgroundColor(0xFFF44336.toInt())
                    resetAmmoButton.alpha = 1.0f

                    resetAmmoButton.postDelayed({
                        if (ammoButtonState == ButtonState.ERROR) {
                            setAmmoButtonState(ButtonState.READY)
                        }
                    }, 3000)
                }
            }
        }

        private fun updatePulseButtonAppearance() {
            val globalEnabled = isButtonsEnabled()
            
            when (pulseButtonState) {
                ButtonState.READY -> {
                    pulseTestButton.isEnabled = globalEnabled
                    pulseTestButton.text = "Pulse Test"
                    pulseTestButton.setBackgroundColor(
                        if (globalEnabled) 0xFF2196F3.toInt() else 0xFFBDBDBD.toInt() // 파란색
                    )
                    pulseTestButton.alpha = if (globalEnabled) 1.0f else 0.6f
                }
                ButtonState.PROCESSING -> {
                    pulseTestButton.isEnabled = false
                    pulseTestButton.text = "테스트 중..."
                    pulseTestButton.setBackgroundColor(0xFF9E9E9E.toInt())
                    pulseTestButton.alpha = 0.8f
                }
                ButtonState.SUCCESS -> {
                    pulseTestButton.isEnabled = globalEnabled
                    pulseTestButton.text = "성공!"
                    pulseTestButton.setBackgroundColor(0xFF4CAF50.toInt())
                    pulseTestButton.alpha = 1.0f

                    pulseTestButton.postDelayed({
                        if (pulseButtonState == ButtonState.SUCCESS) {
                            setPulseButtonState(ButtonState.READY)
                        }
                    }, 2000)
                }
                ButtonState.ERROR -> {
                    pulseTestButton.isEnabled = globalEnabled
                    pulseTestButton.text = "실패"
                    pulseTestButton.setBackgroundColor(0xFFF44336.toInt())
                    pulseTestButton.alpha = 1.0f

                    pulseTestButton.postDelayed({
                        if (pulseButtonState == ButtonState.ERROR) {
                            setPulseButtonState(ButtonState.READY)
                        }
                    }, 3000)
                }
            }
        }

        private fun updateConfigButtonAppearance() {
            val globalEnabled = isButtonsEnabled()
            
            when (configButtonState) {
                ButtonState.READY -> {
                    configDumpButton.isEnabled = globalEnabled
                    configDumpButton.text = "Read"
                    configDumpButton.setBackgroundColor(
                        if (globalEnabled) 0xFF008800.toInt() else 0xFFBDBDBD.toInt() // 녹색
                    )
                    configDumpButton.alpha = if (globalEnabled) 1.0f else 0.6f
                }
                ButtonState.PROCESSING -> {
                    configDumpButton.isEnabled = false
                    configDumpButton.text = "조회 중..."
                    configDumpButton.setBackgroundColor(0xFF9E9E9E.toInt())
                    configDumpButton.alpha = 0.8f
                }
                ButtonState.SUCCESS -> {
                    configDumpButton.isEnabled = globalEnabled
                    configDumpButton.text = "성공!"
                    configDumpButton.setBackgroundColor(0xFF4CAF50.toInt())
                    configDumpButton.alpha = 1.0f

                    configDumpButton.postDelayed({
                        if (configButtonState == ButtonState.SUCCESS) {
                            setConfigButtonState(ButtonState.READY)
                        }
                    }, 2000)
                }
                ButtonState.ERROR -> {
                    configDumpButton.isEnabled = globalEnabled
                    configDumpButton.text = "실패"
                    configDumpButton.setBackgroundColor(0xFFF44336.toInt())
                    configDumpButton.alpha = 1.0f

                    configDumpButton.postDelayed({
                        if (configButtonState == ButtonState.ERROR) {
                            setConfigButtonState(ButtonState.READY)
                        }
                    }, 3000)
                }
            }
        }

        private fun updateWriteButtonAppearance() {
            val globalEnabled = isButtonsEnabled()
            
            when (writeButtonState) {
                ButtonState.READY -> {
                    configSetButton.isEnabled = globalEnabled
                    configSetButton.text = "Write"
                    configSetButton.setBackgroundColor(
                        if (globalEnabled) 0xFFFF0000.toInt() else 0xFFBDBDBD.toInt() // 빨간색
                    )
                    configSetButton.alpha = if (globalEnabled) 1.0f else 0.6f
                }
                ButtonState.PROCESSING -> {
                    configSetButton.isEnabled = false
                    configSetButton.text = "설정 중..."
                    configSetButton.setBackgroundColor(0xFF9E9E9E.toInt())
                    configSetButton.alpha = 0.8f
                }
                ButtonState.SUCCESS -> {
                    configSetButton.isEnabled = globalEnabled
                    configSetButton.text = "성공!"
                    configSetButton.setBackgroundColor(0xFF4CAF50.toInt())
                    configSetButton.alpha = 1.0f

                    configSetButton.postDelayed({
                        if (writeButtonState == ButtonState.SUCCESS) {
                            setWriteButtonState(ButtonState.READY)
                        }
                    }, 2000)
                }
                ButtonState.ERROR -> {
                    configSetButton.isEnabled = globalEnabled
                    configSetButton.text = "실패"
                    configSetButton.setBackgroundColor(0xFFF44336.toInt())
                    configSetButton.alpha = 1.0f

                    configSetButton.postDelayed({
                        if (writeButtonState == ButtonState.ERROR) {
                            setWriteButtonState(ButtonState.READY)
                        }
                    }, 3000)
                }
            }
        }

        fun refreshButtonState() {
            updateAmmoButtonAppearance()
            updatePulseButtonAppearance()
            updateConfigButtonAppearance()
            updateWriteButtonAppearance()
        }

        fun getDevice(): BleDevice? = currentDevice
        fun getCurrentState(): ButtonState = ammoButtonState
        fun getPulseButtonState(): ButtonState = pulseButtonState
        fun getConfigButtonState(): ButtonState = configButtonState
        fun getWriteButtonState(): ButtonState = writeButtonState
    }

    enum class ButtonState {
        READY,      // 준비 상태
        PROCESSING, // 처리 중
        SUCCESS,    // 성공
        ERROR       // 실패
    }

    override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): BleDeviceViewHolder {
        val view = LayoutInflater.from(parent.context)
            .inflate(R.layout.item_ble_device, parent, false)
        return BleDeviceViewHolder(view)
    }

    override fun onBindViewHolder(holder: BleDeviceViewHolder, position: Int) {
        val device = devices[position]
        holder.deviceName.text = device.name
        holder.deviceAddress.text = device.address
        holder.deviceRssi.text = "${device.rssi} dBm"

        // 버튼 상태 초기화
        if (holder.getCurrentState() == ButtonState.READY && 
            holder.getPulseButtonState() == ButtonState.READY && 
            holder.getConfigButtonState() == ButtonState.READY &&
            holder.getWriteButtonState() == ButtonState.READY) {
            holder.setAmmoButtonState(ButtonState.READY)
            holder.setPulseButtonState(ButtonState.READY)
            holder.setConfigButtonState(ButtonState.READY)
            holder.setWriteButtonState(ButtonState.READY)
        } else {
            // 이미 다른 상태라면 현재 상태 유지하면서 활성화 여부만 업데이트
            holder.refreshButtonState()
        }
    }

    override fun getItemCount(): Int = devices.size

    fun addDevice(device: BleDevice) {
        val index = devices.indexOfFirst { it.address == device.address }
        if (index != -1) {
            devices[index] = device
            notifyItemChanged(index)
            Log.d("BleDeviceAdapter", "Device updated at position $index: ${device.name}")
        } else {
            devices.add(device)
            notifyItemInserted(devices.size - 1)
            Log.d("BleDeviceAdapter", "Device added at position ${devices.size - 1}: ${device.name}")
        }
        Log.d("BleDeviceAdapter", "Total devices in list: ${devices.size}")
    }

    fun clearDevices() {
        val previousSize = devices.size
        devices.clear()
        notifyDataSetChanged()
        Log.d("BleDeviceAdapter", "Devices cleared. Previous count: $previousSize, Current count: ${devices.size}")
    }

    // 모든 ViewHolder의 버튼 상태 새로고침
    fun refreshAllButtonStates() {
        notifyDataSetChanged()
    }

    // Ammo Reset 버튼 상태 업데이트
    fun updateAmmoButtonState(deviceAddress: String, state: ButtonState) {
        val layoutManager = recyclerView?.layoutManager as? androidx.recyclerview.widget.LinearLayoutManager
        val position = devices.indexOfFirst { it.address == deviceAddress }
        if (position != -1) {
            // RecyclerView에서 ViewHolder 찾기는 MainActivity에서 처리
            notifyItemChanged(position)
        }
    }

    // Pulse Test 버튼 상태 업데이트
    fun updatePulseButtonState(deviceAddress: String, state: ButtonState) {
        val position = devices.indexOfFirst { it.address == deviceAddress }
        if (position != -1) {
            notifyItemChanged(position)
        }
    }

    // 기존 호환성을 위한 메서드
    fun updateButtonState(deviceAddress: String, state: ButtonState) {
        updateAmmoButtonState(deviceAddress, state)
    }

    private var recyclerView: RecyclerView? = null

    override fun onAttachedToRecyclerView(recyclerView: RecyclerView) {
        super.onAttachedToRecyclerView(recyclerView)
        this.recyclerView = recyclerView
    }
}