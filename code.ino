#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

// 定義 BLE 的專屬 ID
#define SERVICE_UUID           "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID_RX "beb5483e-36e1-4688-b7f5-ea07361b26a8"

bool ebsTriggeredByBLE = false; // 建立一個旗標，紀錄是否收到急停訊號
bool vlsTriggeredByBLE = false; // 建立一個旗標，紀錄是否收到起跑訊號
// 建立接收 BLE 藍牙訊息的回呼函式 (當手機傳訊息來時，這裡會被觸發)
// 建立接收 BLE 藍牙訊息的回呼函式
class MyCallbacks: public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) {
      String rxValue = pCharacteristic->getValue(); 
      if (rxValue.length() > 0) {
        char cmd = rxValue[0]; // 讀取收到的第一個字元
        
        if (cmd == 'S' || cmd == 's') {
          ebsTriggeredByBLE = true; // 收到 'S'，拉起急停旗標！
        } 
        else if (cmd == 'G' || cmd == 'g') {
          vlsTriggeredByBLE = true;  // 收到 'G'，拉起起跑旗標！
          ebsTriggeredByBLE = false; // [新增] 同時把急停狀態解除！
        }
      }
    }
};
// ================= 1. 腳位定義 =================
// ASL 指示燈腳位
const int LED_R = 16; // 紅燈
const int LED_G = 17; // 綠燈
const int LED_B = 18; // 藍燈
// 循跡感測器腳位 (由左至右：S1 到 S5)
const int S1 = 36; // VP, 最左側感測器
const int S2 = 39; // VN, 左側感測器
const int S3 = 34; // 中間感測器
const int S4 = 35; // 右側感測器
const int S5 = 32; // 最右側感測器

// L298N 馬達控制腳位
const int IN1 = 25; // 左後輪方向
const int IN2 = 26; // 左後輪方向
const int ENA = 33; // 左後輪速度 (PWM)

const int IN3 = 27; // 右後輪方向
const int IN4 = 14; // 右後輪方向
const int ENB = 13; // 右後輪速度 (PWM)

// ================= 2. 參數設定 =================

// 定義車速 (範圍 0 - 255)
const int BASE_SPEED = 216;  

// -- 小彎微調參數 --
const int MINOR_FAST = 178;  
const int MINOR_SLOW = 83;   

// -- 直角彎急轉參數--
const int SHARP_FAST = 255;  // 外側輪胎全速推進
const int SHARP_SLOW = -255;// ★ 內側輪胎「倒退」，強制把車頭扯回來

// ================= ASL 燈號控制副程式 =================
void setASL(String color) {
  // 先把所有燈關掉
  digitalWrite(LED_R, LOW);
  digitalWrite(LED_G, LOW);
  digitalWrite(LED_B, LOW);

  // 根據指令亮起對應的燈
  if (color == "GREEN") {
    digitalWrite(LED_G, HIGH);
  } else if (color == "RED") {
    digitalWrite(LED_R, HIGH);
  } else if (color == "BLUE") {
    digitalWrite(LED_B, HIGH);
  }
}
// ================= 3. 初始化設定 =================
void setup() {
  Serial.begin(115200);
  // 設定 ASL 燈號為輸出模式
  pinMode(LED_R, OUTPUT);
  pinMode(LED_G, OUTPUT);
  pinMode(LED_B, OUTPUT);
  // 啟動藍牙
// --- 啟動 BLE 藍牙 ---
  Serial.println("啟動 BLE 藍牙中...");
  BLEDevice::init("NCKU_Tracking_ChunYushi"); // 手機搜尋時會看到這個名字
  BLEServer *pServer = BLEDevice::createServer();
  BLEService *pService = pServer->createService(SERVICE_UUID);
  
 // 建立接收通道，並綁定剛剛寫好的回呼函式
  BLECharacteristic *pCharacteristic = pService->createCharacteristic(
                                         CHARACTERISTIC_UUID_RX,
                                         BLECharacteristic::PROPERTY_WRITE
                                       );
  pCharacteristic->setCallbacks(new MyCallbacks());
  
  pService->start();
  pServer->getAdvertising()->start();
  Serial.println("BLE 藍牙已啟動！請用 iPhone 的 LightBlue App 連線");
  // 設定感測器為輸入模式
  pinMode(S1, INPUT);
  pinMode(S2, INPUT);
  pinMode(S3, INPUT);
  pinMode(S4, INPUT);
  pinMode(S5, INPUT);

  // 設定馬達方向控制腳位為輸出模式
  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT);
  pinMode(IN4, OUTPUT);

  // 設定 PWM 腳位為輸出模式
  pinMode(ENA, OUTPUT);
  pinMode(ENB, OUTPUT);
  
  Serial.println("System Ready! 準備進入 VLS 觸發程序...");
  delay(1000); // 開機後稍微緩衝一下
  //ASL：等待起跑，處於安全狀態，亮綠燈！
  setASL("GREEN");
  
  // VLS 藍牙觸發核心邏輯 
  Serial.println("等待 VLS 觸發：請用手機發送 'G' 啟動車輛...");
  bool isVlsTriggered = false; // 建立一個狀態旗標，紀錄是否已經起跑
  // VLS 觸發陷阱：只要還沒收到 'G'，就一直卡在這個迴圈裡，車子絕對不會動
  while (vlsTriggeredByBLE == false) {
    delay(50); // 稍微休息一下，等待手機藍牙發送訊號
  }
  
  // 只要程式走到這裡，代表手機已經成功發送了 'G'
  Serial.println(">>> 收到藍牙起跑訊號！起跑倒數 3 秒 <<<");
  delay(1000);
  Serial.println("2...");
  delay(1000);
  Serial.println("1...");
  delay(1000);
  Serial.println("GO!");
  
  // ASL：車輛起跑進入自主導航狀態，切換成紅燈！
  setASL("RED");
  // VLS 藍牙觸發結束 
}

// ================= 4. 馬達控制模組 =================

/*

 * 控制左右輪速度與方向的專用副程式

 * 傳入正數代表前進，負數代表後退，0 代表停止

 * 數值範圍：-255 到 255

 */

void setMotor(int leftSpeed, int rightSpeed) {

  // --- 控制左輪 ---

  if (leftSpeed > 0) {

    digitalWrite(IN1, HIGH);

    digitalWrite(IN2, LOW);

    analogWrite(ENA, leftSpeed);

  } else if (leftSpeed < 0) {

    digitalWrite(IN1, LOW);

    digitalWrite(IN2, HIGH);

    analogWrite(ENA, -leftSpeed); // 轉為正數給 PWM

  } else {

    digitalWrite(IN1, LOW);

    digitalWrite(IN2, LOW);

    analogWrite(ENA, 0);

  }



  // --- 控制右輪 ---

  if (rightSpeed > 0) {

    digitalWrite(IN3, HIGH);

    digitalWrite(IN4, LOW);

    analogWrite(ENB, rightSpeed);

  } else if (rightSpeed < 0) {

    digitalWrite(IN3, LOW);

    digitalWrite(IN4, HIGH);

    analogWrite(ENB, -rightSpeed);

  } else {

    digitalWrite(IN3, LOW);

    digitalWrite(IN4, LOW);

    analogWrite(ENB, 0);

  }

}

// ================= 5. 主迴圈 (循跡邏輯) =================

void loop() {
  // ---------------------------------------------------------
  // EBS 第一層：BLE 藍牙外部急停接收區 
  // ---------------------------------------------------------
  // 檢查急停旗標是否被 BLE 觸發
  // ---------------------------------------------------------
  // EBS 第一層：BLE 藍牙外部急停接收區 
  // ---------------------------------------------------------
  // 檢查急停旗標是否被 BLE 觸發
  if (ebsTriggeredByBLE == true) {
    Serial.println("!!! 收到 BLE 藍牙 EBS 急停訊號，車輛暫停 !!!");
    setMotor(0, 0); // 扭矩立刻歸零
    
    // ASL：觸發緊急制動，切換成藍燈！
    setASL("BLUE");
    
    // [關鍵修改] 只要 ebsTriggeredByBLE 還是 true，就一直卡在這裡
    // 等到手機發送 'G'，上面的程式會把它變成 false，就會自動打破這個迴圈！
    while(ebsTriggeredByBLE == true) {
      delay(50); // 處於藍燈暫停狀態
    }
    
    // --- 只要能走到這裡，代表手機發送了 'G' 解除鎖定 ---
    Serial.println(">>> 收到恢復訊號！繼續自主導航 <<<");
    setASL("RED"); // ASL：恢復成自主導航的紅燈
    delay(1000);   // 給 1 秒鐘的緩衝時間，再讓車子衝出去
  }
  // ---------------------------------------------------------
  // ---------------------------------------------------------
  // ---------------------------------------------------------
  int val1 = digitalRead(S1);
  int val2 = digitalRead(S2);
  int val3 = digitalRead(S3);
  int val4 = digitalRead(S4);
  int val5 = digitalRead(S5);

  
  // ---------------------------------------------------------

  // 循跡判斷邏輯
  
  // 狀況 1：遇到直角彎偏右 (最左側壓線) -> 最優先處理！暴力急左轉！
  if (val1 == HIGH && val5==LOW) {
    setMotor(SHARP_SLOW, SHARP_FAST); // 左輪倒退，右輪全速
  }
  // 狀況 2：遇到直角彎偏左 (最右側壓線) -> 最優先處理！暴力急右轉！
  else if (val5 == HIGH && val1==LOW) {
    setMotor(SHARP_FAST, SHARP_SLOW); // 左輪全速，右輪倒退
  }
  // 狀況 3：偏右一點點 -> 溫柔向左微調
  else if (val2 == HIGH) {
    setMotor(MINOR_SLOW, MINOR_FAST);
  }
  // 狀況 4：偏左一點點 -> 溫柔向右微調
  else if (val4 == HIGH) {
    setMotor(MINOR_FAST, MINOR_SLOW);
  }
  // 狀況 5：旁邊都沒事，只有中間壓線 -> 全速直衝！
  else if (val3 == HIGH) {
    setMotor(BASE_SPEED, BASE_SPEED); 
  }
  // 狀況 6：完全迷失 (全白)
  else {
    setMotor(100, 100);
  }

  delay(5); 
}
