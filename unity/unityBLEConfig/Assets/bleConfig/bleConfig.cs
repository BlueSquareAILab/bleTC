using System.Collections.Generic;
using UnityEngine;
using UnityEngine.UI;
using TMPro;
using System.Text;
using System.Threading.Tasks;


public class BleScanner : MonoBehaviour
{
    public bool isScanningDevices = false;
    public bool isScanningServices = false;
    public bool isScanningCharacteristics = false;
    public bool isSubscribed = false;

    [SerializeField] Button btnScan;
    [SerializeField] Button btnConnect;
    [SerializeField] Button btnAbout;
    [SerializeField] Button btnStatus;
    // [SerializeField] Button btnClose;
    [SerializeField] Button btnWriteConfig;
    [SerializeField] Button btnReadConfig;
    [SerializeField] TMP_InputField infDeBounce;
    [SerializeField] TMP_Dropdown dropdownDevices;
    [SerializeField] TMP_Text txStatusLog;
    [SerializeField] TMP_Text txCounter;
    [SerializeField] Button btnClearCounter;
    // [SerializeField] TMP_Dropdown dropdown_PortList;

    public string selectedDeviceId;
    public string selectedServiceId;
    Dictionary<string, string> characteristicNames = new Dictionary<string, string>();
    public string selectedCharacteristicId;
    Dictionary<string, Dictionary<string, string>> devices = new Dictionary<string, Dictionary<string, string>>();
    string lastError;

    const string SERVICE_UUID = "2ca354b0-5f62-11ef-b4d4-f7af9038ee7d";
    const string CHARACTERISTIC_UUID = "35c34c80-5f62-11ef-b4d4-f7af9038ee7d";


    // Start is called before the first frame update
    void Start()
    {
        // scanResultRoot = deviceScanResultProto.transform.parent;
        // deviceScanResultProto.transform.SetParent(null);

        selectedCharacteristicId = CHARACTERISTIC_UUID;
        selectedServiceId = SERVICE_UUID;

        btnScan.onClick.AddListener(StartStopDeviceScan);


        btnConnect.onClick.AddListener(() =>
        {
            if(isSubscribed) {
                // BleApi.UnsubscribeCharacteristic(selectedDeviceId, selectedServiceId, selectedCharacteristicId, false);
                // isSubscribed = false;
                BleApi.DisconnectDevice(selectedDeviceId);
                isSubscribed = false;

                btnConnect.GetComponentInChildren<TextMeshProUGUI>().text = "Connect";
            }
            else {
                string selectedDevice = dropdownDevices.options[dropdownDevices.value].text;
                string selectId = "";

                foreach (var device in devices)
                {
                    if (device.Value["name"] == selectedDevice)
                    {
                        selectId = device.Key;
                        break;
                    }
                }

                if (selectId != null && selectId != "")
                {
                    selectedDeviceId = selectId;
                    Debug.Log($"Connect button clicked: {selectedDeviceId}");

                    // no error code available in non-blocking mode
                    BleApi.SubscribeCharacteristic(selectedDeviceId, selectedServiceId, selectedCharacteristicId, false);
                    isSubscribed = true;
                }

                btnConnect.GetComponentInChildren<TextMeshProUGUI>().text = "Disconnect";
            }
            
            
        });

        btnAbout.onClick.AddListener(() =>
        {

            txStatusLog.text = "";
            // Debug.Log("About button clicked");
            Write("about");
        });


        btnReadConfig.onClick.AddListener(() =>
        {
            // Debug.Log("Read Config button clicked");
            Write("config dump");
        });

        btnWriteConfig.onClick.AddListener(() =>
        {
            // Debug.Log("Write Config button clicked");
            Write("config set debounceDelay " + infDeBounce.text);
            Write("config save");

            infDeBounce.text = "";
        });


        btnClearCounter.onClick.AddListener(() =>
        {
            // Debug.Log("Clear Counter button clicked");
            Write("clear");
        });

        btnStatus.onClick.AddListener(() =>
        {
            // Debug.Log("Status button clicked");
            Write("status");
        });
    }

    bool CheckKey(string jsonString,string _key)
    {
        // 찾을 키
        string key = $"\"{_key}\":\"";

        // 'debounceDelay' 키의 시작 위치 찾기
        int keyIndex = jsonString.IndexOf(key);
        if (keyIndex != -1)
        {
            return true;
        }
        else
        {
            return false;
        }
    }
    int ExtractDebounceDelay(string jsonString)
    {
        // 찾을 키
        string key = "\"debounceDelay\":\"";
        int debounceDelay = 0;

        // 'debounceDelay' 키의 시작 위치 찾기
        int keyIndex = jsonString.IndexOf(key);
        if (keyIndex != -1)
        {
            // 키의 끝 위치에서 값의 시작 위치 계산
            int valueStartIndex = keyIndex + key.Length;

            // 값의 끝 위치 찾기 (따옴표로 끝나는 값)
            int valueEndIndex = jsonString.IndexOf("\"", valueStartIndex);

            if (valueEndIndex != -1)
            {
                // 값 추출
                string valueString = jsonString.Substring(valueStartIndex, valueEndIndex - valueStartIndex);

                // 문자열 값을 정수로 변환
                if (int.TryParse(valueString, out debounceDelay))
                {
                    Debug.Log($"Parsed debounceDelay: {debounceDelay}");
                }
                else
                {
                    Debug.LogError("Failed to parse debounceDelay as an integer.");
                }
            }
            else
            {
                Debug.LogError("Closing quote for debounceDelay value not found.");
            }
        }
        else
        {
            Debug.LogError("debounceDelay key not found in the JSON string.");
        }

        return debounceDelay;
    }

    // Update is called once per frame
    void Update()
    {
        BleApi.ScanStatus status;
        if (isScanningDevices)
        {
            BleApi.DeviceUpdate res = new BleApi.DeviceUpdate();
            do
            {
                status = BleApi.PollDevice(ref res, false);
                if (status == BleApi.ScanStatus.AVAILABLE)
                {
                    if (!devices.ContainsKey(res.id)) {
                        Debug.Log($"장치 발견: {res.name}, 주소: {res.id}, 연결 가능: {res.isConnectable}");

                        devices[res.id] = new Dictionary<string, string>()
                        {
                            { "name", res.name },
                            { "isConnectable", res.isConnectable.ToString() }
                        };

                        if (res.name != null && res.name != "")
                            dropdownDevices.options.Add(new TMP_Dropdown.OptionData(res.name));
                            dropdownDevices.RefreshShownValue();

                    }
                        // devices[res.id] = new Dictionary<string, string>() {
                        //     { "name", "" },
                        //     { "isConnectable", "False" }
                        // };
                    if (res.nameUpdated)
                        devices[res.id]["name"] = res.name;
                    if (res.isConnectableUpdated)
                        devices[res.id]["isConnectable"] = res.isConnectable.ToString();
                    
                }
                else if (status == BleApi.ScanStatus.FINISHED)
                {
                    isScanningDevices = false;
                    // deviceScanButtonText.text = "Scan devices";
                    // deviceScanStatusText.text = "finished";
                }
            } while (status == BleApi.ScanStatus.AVAILABLE);
        }
        
        if (isSubscribed)
        {
            BleApi.BLEData res = new BleApi.BLEData();
            while (BleApi.PollData(out res, false))
            {
                Debug.Log("Received data: " + Encoding.ASCII.GetString(res.buf, 0, res.size));

                // # 로 시작하는 문자열인지 확인
                if(Encoding.ASCII.GetString(res.buf, 0, 1) == "#")
                {
                    var receivedData = Encoding.ASCII.GetString(res.buf, 1, res.size - 1);
                     // ','를 기준으로 문자열 분리
                    string[] parts = receivedData.Split(',');

                    if (int.TryParse(parts[1], out int counter))
                    {
                        Debug.Log($"Counter: {counter}");
                        txCounter.text = counter.ToString();
                    }
                    
                }
                else {

                    //json 파싱
                    var json = Encoding.ASCII.GetString(res.buf, 0, res.size);

                    if(CheckKey(json, "debounceDelay"))
                    {
                        int debounceDelay = ExtractDebounceDelay(json);

                        Debug.Log($"debounceDelay: {debounceDelay}");

                        infDeBounce.text = debounceDelay.ToString();
                    }
                    
                    txStatusLog.text += Encoding.ASCII.GetString(res.buf, 0, res.size);
                    txStatusLog.text += "\n";
                }
                
            }
        }
        {
            // log potential errors
            BleApi.ErrorMessage res = new BleApi.ErrorMessage();
            BleApi.GetError(out res);
            if (lastError != res.msg)
            {
                if(res.msg == "Ok") {
                    Debug.Log("Error Log start");
                } else {
                    Debug.LogError(res.msg);
                }
                // errorText.text = res.msg;
                lastError = res.msg;
            }
        }
    }

    private void OnApplicationQuit()
    {
        Debug.Log("Application Quit");
        BleApi.Quit();
    }

    public void StartStopDeviceScan()
    {
        TextMeshProUGUI btnScanText = btnScan.GetComponentInChildren<TextMeshProUGUI>();
        if (!isScanningDevices)
        {
            // start new scan
            // for (int i = scanResultRoot.childCount - 1; i >= 0; i--)
            //     Destroy(scanResultRoot.GetChild(i).gameObject);

            //dropdownDevices.ClearOptions();
            dropdownDevices.options = new List<TMP_Dropdown.OptionData>();
            dropdownDevices.options.Clear();
            devices.Clear();

            BleApi.StartDeviceScan();
            isScanningDevices = true;

            btnScanText.text = "Stop scan";
        }
        else
        {
            // stop scan
            isScanningDevices = false;
            BleApi.StopDeviceScan();

            btnScanText.text = "Start scan";
        }
    }

    

    public void Write(string text)
    {
        byte[] payload = Encoding.ASCII.GetBytes(text);
        BleApi.BLEData data = new BleApi.BLEData();
        data.buf = new byte[512];
        data.size = (short)payload.Length;
        data.deviceId = selectedDeviceId;
        data.serviceUuid = selectedServiceId;
        data.characteristicUuid = selectedCharacteristicId;
        for (int i = 0; i < payload.Length; i++)
            data.buf[i] = payload[i];
        // no error code available in non-blocking mode
        BleApi.SendData(in data, false);
    }


}
