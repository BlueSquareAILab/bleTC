package com.example.bsmagapp

import android.bluetooth.BluetoothDevice

data class BleDevice(
    val device: BluetoothDevice,
    val rssi: Int,
    val scanRecord: ByteArray?,
    val originalName: String? = null  // 스캔 시점의 원본 이름 저장
) {
    val name: String
        get() = originalName ?: device.name ?: "Unknown BSQTC Device"

    val address: String
        get() = device.address

    // BSQTC 디바이스인지 확인하는 함수
    fun isBSQTCDevice(): Boolean {
        return originalName?.startsWith("BSQTC", ignoreCase = true) == true ||
               device.name?.startsWith("BSQTC", ignoreCase = true) == true
    }

    override fun equals(other: Any?): Boolean {
        if (this === other) return true
        if (javaClass != other?.javaClass) return false
        other as BleDevice
        return device.address == other.device.address
    }

    override fun hashCode(): Int {
        return device.address.hashCode()
    }
}