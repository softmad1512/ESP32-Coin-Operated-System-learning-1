#include <Arduino.h>    // 主程式
#include <Wire.h>       // I2C用
#include <Adafruit_GFX.h>       // OLED顯示用
#include <Adafruit_SSD1306.h>   // OLED顯示用

// 腳位 //
#define COIN_PIN 27     // 投幣訊號
#define RELAY_PIN 26    // 控制繼電器

// OLED設定
#define SCREEN_WIDTH 128    // 螢幕寬度解析度
#define SCREEN_HEIGHT 64    // 螢幕高度解析度

// 設定解析度 & 使用I2C通訊 & 沒有reset鍵
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// 設備狀態 //
enum SystemState
{
    WAIT_COIN,RELAY_ACTIVE  // 投幣監控,繼電器動作(2個狀態)
};

SystemState state = WAIT_COIN;  // 設定最初狀態

// 變數定義 //
int totalCoin = 0;      // 總投幣數
int machineCoin = 0;    // 設備投幣紀錄
int game_cost = 5;     // 遊戲需求幣數
volatile int coinsignal = 0;
unsigned long lastCoinTime = 0;     // 記時+debounce用
unsigned long relayStartTime = 0;   // 記時+relay啟動時間
int lastDisplayCoin = -1;   // 最新的一次投幣數、(-1)為給範圍外初始值避免OLED重複刷新

//投幣ISR
void IRAM_ATTR coinISR()
{
    coinsignal++;    //投幣偵測
}

// 設定初始 //
void setup() 
{
    Serial.begin(115200);   // 啟動序列通訊（Debug用）
    pinMode(COIN_PIN, INPUT_PULLUP);    // 輸入：投幣訊號(高電平)
    pinMode(RELAY_PIN, OUTPUT);         // 輸出：繼電器訊號
    digitalWrite(RELAY_PIN, LOW);       // 繼電器訊號（低電平）

    //偵測投幣電平下降時執行coinISR
    attachInterrupt(digitalPinToInterrupt(COIN_PIN), coinISR, FALLING);  
    
    //OLED初始
    //判斷動作(失敗時)：螢幕連線(I2C：0x3C)
    if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C))
    {
        Serial.println("OLED init failed"); //顯示啟動失敗
        while (true);   //用迴圈將程式停在啟動失敗
    }

    display.clearDisplay();         //清空顯示畫面
    display.setTextSize(2);         //字體大小
    display.setTextColor(WHITE);    //字體顏色
}

// 主程式 //
//投幣監控+防抖動(Debounce)
void loop() {
    while (coinsignal>0) 
    {
        coinsignal--;  // 回到期初狀態
        // 當下時間-上次投幣時間>200ms時才執行(Debounce：200ms)
        if (millis() - lastCoinTime > 200) 
        {
            totalCoin++;                // 投幣 +1
            machineCoin++;              // 設備投幣紀錄 +1
            lastCoinTime = millis();    // 更新最新投幣時間紀錄

            // 印出資訊：Coin:(totalCoin)
            Serial.print("Coin: ");
            Serial.println(totalCoin);
            // 印出資訊：machineCoin:(machineCoin)
            Serial.print("machineCoin: ");
            Serial.println(machineCoin);
            }
    }
    // 設備狀態控制
    switch (state) 
    {
        case WAIT_COIN:
            // 投幣監控：當投幣訊號>=遊戲需求幣數 → 啟動繼電器
            if (totalCoin >= game_cost)
            {
                // 繼電器輸出高電平(啟動)
                Serial.println("Relay ON");
                digitalWrite(RELAY_PIN, HIGH);   
                // 記錄當下時間
                relayStartTime = millis();       
                // 切換設備狀態(繼電器動作)
                state = RELAY_ACTIVE;            
            }
            //離開[if (totalCoin >= 5)]判斷式
            break;
            
        case RELAY_ACTIVE:
            // 當下時間-繼電器啟動時間>3000ms
            if (millis() - relayStartTime > 3000) 
            {
                // 繼電器輸出低電平(關閉)
                Serial.println("Relay OFF");
                digitalWrite(RELAY_PIN, LOW);  
                // 扣除遊戲需求幣數並更新總投幣數
                totalCoin-=game_cost;
                // 切換設備狀態(投幣監控)
                state = WAIT_COIN;
            }
            //離開[if (millis() - relayStartTime > 3000)]判斷式
            break; 
    }   
    // OLED更新
    // 總投幣數≠最新投幣
    if (totalCoin != lastDisplayCoin) 
    {
        display.clearDisplay();     // 清空OLED畫面
        display.setCursor(0, 10);   // 文字輸入座標(x,y)
        display.print("Coin:");     // 顯示Coin:
        display.setCursor(0, 30);   // 文字輸入座標(x,y) 
        display.print(totalCoin);   // 顯示totalCoin值
        display.display();          // 執行上述print內容
        lastDisplayCoin = totalCoin;// 同步:總投幣數=最新投幣
    }
    // loop監控緩衝（避免CPU滿載）
    delay(10);
}