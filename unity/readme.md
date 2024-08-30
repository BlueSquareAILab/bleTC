# Unity에서의 Windows BLE 구현 가이드

## 1 프로젝트 설정

1. Unity 프로젝트 생성 또는 열기
2. File > Build Settings에서 플랫폼을 PC, Mac & Linux Standalone으로 설정
3. Target Platform을 Windows로 설정
4. Player Settings > Other Settings > Configuration > Scripting Backend를 IL2CPP로 설정
5. Player Settings > Other Settings > Configuration > API Compatibility Level을 .NET 4.x로 설정

## 2 Windows Runtime 지원 추가

1. 새 C# 스크립트 `WindowsRuntimeSupport.cs` 생성
2. 다음 코드 추가:

```csharp
using UnityEngine;
using System.Runtime.InteropServices;

public class WindowsRuntimeSupport : MonoBehaviour
{
    [DllImport("kernel32.dll", SetLastError = true)]
    static extern bool AllocConsole();

    [RuntimeInitializeOnLoadMethod(RuntimeInitializeLoadType.BeforeSceneLoad)]
    static void Init()
    {
        AllocConsole();
        var folder = System.Environment.GetFolderPath(System.Environment.SpecialFolder.LocalApplicationData);
        var exePath = System.IO.Path.Combine(folder, @"UnityPlayer_Data\Plugins\x86_64\UnityPlayer.dll");
        LoadPackagedLibrary(exePath);
    }

    [DllImport("kernel32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
    static extern IntPtr LoadPackagedLibrary(string lpFileName, uint dwFlags = 0);
}
```

## 3 BLE 매니저 구현

새로운 C# 스크립트 `BLEManager.cs`를 생성하고 다음과 같이 구현:

```csharp
using UnityEngine;
using System;
using System.Threading.Tasks;
using Windows.Devices.Bluetooth;
using Windows.Devices.Bluetooth.GenericAttributeProfile;
using Windows.Storage.Streams;

public class BLEManager : MonoBehaviour
{
    private BluetoothLEDevice bluetoothLeDevice;
    private GattCharacteristic characteristic;

    public async Task ConnectToDevice(string deviceId)
    {
        bluetoothLeDevice = await BluetoothLEDevice.FromIdAsync(deviceId);
        var services = await bluetoothLeDevice.GetGattServicesAsync();
        foreach (var service in services.Services)
        {
            if (service.Uuid == Guid.Parse("2ca354b0-5f62-11ef-b4d4-f7af9038ee7d"))
            {
                var characteristics = await service.GetCharacteristicsAsync();
                foreach (var c in characteristics.Characteristics)
                {
                    if (c.Uuid == Guid.Parse("35c34c80-5f62-11ef-b4d4-f7af9038ee7d"))
                    {
                        characteristic = c;
                        break;
                    }
                }
                break;
            }
        }
    }

    public async Task SendCommand(string command)
    {
        var writer = new DataWriter();
        writer.WriteString(command);
        await characteristic.WriteValueAsync(writer.DetachBuffer());
    }

    public async Task<string> ReadValue()
    {
        var result = await characteristic.ReadValueAsync();
        var reader = DataReader.FromBuffer(result.Value);
        return reader.ReadString(reader.UnconsumedBufferLength);
    }

    public void StartNotifications(TypedEventHandler<GattCharacteristic, GattValueChangedEventArgs> handler)
    {
        characteristic.ValueChanged += handler;
        characteristic.WriteClientCharacteristicConfigurationDescriptorAsync(
            GattClientCharacteristicConfigurationDescriptorValue.Notify);
    }
}
```

## 4 총기 컨트롤러 구현

`GunController.cs` 스크립트를 생성하고 다음과 같이 구현:

```csharp
using UnityEngine;
using System;
using Windows.Devices.Bluetooth.GenericAttributeProfile;
using Windows.Storage.Streams;

public class GunController : MonoBehaviour
{
    private BLEManager bleManager;

    private async void Start()
    {
        bleManager = gameObject.AddComponent<BLEManager>();
        await bleManager.ConnectToDevice("DeviceId"); // 실제 디바이스 ID로 대체
        bleManager.StartNotifications(OnCharacteristicValueChanged);
        await bleManager.SendCommand("clear");
    }

    private void OnCharacteristicValueChanged(GattCharacteristic sender, GattValueChangedEventArgs args)
    {
        var reader = DataReader.FromBuffer(args.CharacteristicValue);
        var value = reader.ReadString(reader.UnconsumedBufferLength);
        
        if (value.StartsWith("#"))
        {
            var parts = value.Split(',');
            int triggerCount = int.Parse(parts[1]);
            Debug.Log($"Trigger Count: {triggerCount}");
            // 여기에 총기 발사 로직 구현
        }
    }

    private async void OnApplicationQuit()
    {
        if (bleManager != null)
        {
            await bleManager.SendCommand("clear");
        }
    }
}
```

## 5. 데이터 처리

1. 트리거 이벤트:
   - Notify를 통해 "#,count,0,0,0,0,0" 형식의 문자열을 수신합니다.
   - 두 번째 필드(count)가 트리거 카운트입니다.

2. 상태 조회:
   - "status" 명령어를 전송하고 JSON 응답을 파싱합니다.

3. 초기화:
   - 게임 시작 시 "clear" 명령어를 전송하여 카운트를 초기화합니다.