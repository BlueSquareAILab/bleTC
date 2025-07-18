package com.example.bsmagapp

import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.Button
import android.widget.TextView
import androidx.recyclerview.widget.RecyclerView

class BleDeviceAdapter(
    private val devices: MutableList<BleDevice>,
    private val onDeviceClick: (BleDevice) -> Unit,
    private val onAmmoReset: (BleDevice) -> Unit
) : RecyclerView.Adapter<BleDeviceAdapter.BleDeviceViewHolder>() {

    inner class BleDeviceViewHolder(itemView: View) : RecyclerView.ViewHolder(itemView) {
        val deviceName: TextView = itemView.findViewById(R.id.deviceName)
        val deviceAddress: TextView = itemView.findViewById(R.id.deviceAddress)
        val deviceRssi: TextView = itemView.findViewById(R.id.deviceRssi)
        val resetAmmoButton: Button = itemView.findViewById(R.id.resetAmmoButton)

        private var currentDevice: BleDevice? = null

        init {
            itemView.setOnClickListener {
                val position = adapterPosition
                if (position != RecyclerView.NO_POSITION) {
                    onDeviceClick(devices[position])
                }
            }

            resetAmmoButton.setOnClickListener {
                val position = adapterPosition
                if (position != RecyclerView.NO_POSITION) {
                    val device = devices[position]
                    currentDevice = device

                    // 버튼 상태를 처리 중으로 변경
                    setButtonState(ButtonState.PROCESSING)

                    onAmmoReset(device)
                }
            }
        }

        fun setButtonState(state: ButtonState) {
            when (state) {
                ButtonState.READY -> {
                    resetAmmoButton.isEnabled = true
                    resetAmmoButton.text = "Ammo Reset"
                    resetAmmoButton.setBackgroundColor(0xFFFF5722.toInt()) // 주황색
                }
                ButtonState.PROCESSING -> {
                    resetAmmoButton.isEnabled = false
                    resetAmmoButton.text = "처리 중..."
                    resetAmmoButton.setBackgroundColor(0xFF9E9E9E.toInt()) // 회색
                }
                ButtonState.SUCCESS -> {
                    resetAmmoButton.isEnabled = true
                    resetAmmoButton.text = "성공!"
                    resetAmmoButton.setBackgroundColor(0xFF4CAF50.toInt()) // 녹색

                    // 2초 후 원래 상태로 복원
                    resetAmmoButton.postDelayed({
                        setButtonState(ButtonState.READY)
                    }, 2000)
                }
                ButtonState.ERROR -> {
                    resetAmmoButton.isEnabled = true
                    resetAmmoButton.text = "실패"
                    resetAmmoButton.setBackgroundColor(0xFFF44336.toInt()) // 빨간색

                    // 3초 후 원래 상태로 복원
                    resetAmmoButton.postDelayed({
                        setButtonState(ButtonState.READY)
                    }, 3000)
                }
            }
        }

        fun getDevice(): BleDevice? = currentDevice
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
        holder.setButtonState(ButtonState.READY)
    }

    override fun getItemCount(): Int = devices.size

    fun addDevice(device: BleDevice) {
        val index = devices.indexOfFirst { it.address == device.address }
        if (index != -1) {
            devices[index] = device
            notifyItemChanged(index)
        } else {
            devices.add(device)
            notifyItemInserted(devices.size - 1)
        }
    }

    fun clearDevices() {
        devices.clear()
        notifyDataSetChanged()
    }

    // 특정 디바이스의 버튼 상태 업데이트
    fun updateButtonState(deviceAddress: String, state: ButtonState) {
        val position = devices.indexOfFirst { it.address == deviceAddress }
        if (position != -1) {
            val holder = getViewHolderForPosition(position)
            holder?.setButtonState(state)
        }
    }

    // ViewHolder 참조를 위한 헬퍼 함수 (RecyclerView에서 직접 접근)
    private fun getViewHolderForPosition(position: Int): BleDeviceViewHolder? {
        // 이 부분은 MainActivity에서 직접 처리하는 것이 더 안전합니다
        return null
    }
}