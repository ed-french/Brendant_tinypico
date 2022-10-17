/*

        Brendant emergency alarm
        ========================


        Sleeps most of the time
        When manually reset, and every hour,
        it sends to a predefined url
        a get request including:
            server_key - a unique key to exclude any unwanted traffic
            message - a json message with battery status and reset reason
            chip_id - the esp's chip id


        By using a manual reset button, the server can distinguish
        the type of reboot and therefore determine if the alarm
        has been triggered through that button press.

        To start off wifi credentials are hard-coded.

        It is designed to use the tinyPICO, for small size and 
        low sleep current and ability to charge a lithium cell.

        It is intended to figure out a lithium cell suitable for
        about 50 hours of runtime, and embed a qi charging receiver
        so that it could eventually be fully sealed and
        there are no fiddly micro-usb connectors to wrestle-with.

        Ultimately it would be good to include an i2s microphone and
        figure out how to stream audio back to the server to help with
        any false alarms...

        Ultimately, ultimately, there's no reason we couldn't include
        a tiny speaker for reassurance.

        For now a ws2812 multicolour led is used to acknowledge what's happening:

        e.g.
        
        connecting- yellow flashing
        sending regular message - green brief flash
        sending alert message - red brief flash
        help on the way - 20 seconds of blue flashing (once server ack's)
        help not on the way- i.e. fail to send - red flashing for 10 seconds then auto re-try








*/



#include <Arduino.h>
#include <TinyPICO.h>
#include <WiFi.h>
#include <HTTPClient.h>

#include "credentials.h"
/*
    credentials.h needs to define the following

    const char * server_url="http://....";
    const char * wifi_ssid="blah....";
    const char * wifi_password="blah...";
    const char * server_key="blah....";
    const char * hostname="brendant";

*/
#define PIN_USB_CONNECTED 9
#define USB_CONNECTED true


#define BOOT_PIXEL_COLOR 0x004000 // Dimish green
#define NO_WIFI_PIXEL_COLOR 0xA00000 // Strong red
#define SLEEP_AFTER_NO_WIFI_S 20 // Try again sooner if last time didn't connect
#define WIFI_CONNECTED_FAST_PIXEL_COLOR BOOT_PIXEL_COLOR
#define NORMAL_WAKE_SENT_OK_PIXEL_COLOR BOOT_PIXEL_COLOR
#define NORMAL_SLEEP_S 3600 // How long to sleep before sending again, will be 3600
#define HELP_ON_WAY_COLOUR 0x0000FF
#define WIFI_CONNECT_TIMEOUT 20 // Max each try spends trying to connect
#define WIFI_CONNECTING_PIXEL_COLOUR 0x400040


#define uS_TO_S_FACTOR 1000000

TinyPICO tp = TinyPICO();



char reset_reasons[]="" // Each line is 53 characters ;-)
"0. Vbat power on reset                               "
"1. Software reset digital core                       "
"2. Unexpected code number!                           "
"3. Legacy watch dog reset digital core               "
"4. Deep Sleep reset digital core                     "
"5. Reset by SLC module, reset digital core           "
"6. Timer Group0 Watch dog reset digital core         "
"7. Timer G roup1 Watch dog reset digital core        "
"8. RTC Watch dog Reset digital core                  "
"9. Instrusion tested to reset CPU                    "
"10.Time Group reset CPU                              "
"11.Software reset CPU                                "
"12.RTC Watch dog Reset CPU                           "
"13.for APP CPU, reseted by PRO CPU                   "
"14.Reset when the vdd voltage is not stable          "
"15.RTC Watch dog reset digital core and rtc module   "
"01234567890123456789012345678901234567890123456789012";



char message_text_json[200];
// e.g. {"reset_reason":"01234567890123456789012345678901234567890123456789","battery_voltage":3.215,"usb_connected":false}


bool send_status_message(char * json_message)
{
  char url[500]; // plenty of space to build the url with params
  char temp[200];
  strcpy(url,server_url);
  strcat(url,"?chip_id=");
  snprintf(temp, 23, "%llX", ESP.getEfuseMac());
  strcat(url,temp);
  strcat(url,"&message=");
  // escape the json message
  strcpy(temp,json_message); 
  for (uint16_t i=0;i<strlen(temp);i++)
  {
    if (temp[i]==' ') temp[i]='+';
  }
  strcat(url,temp);
  strcat(url,"&server_key=");
  strcat(url,server_key);

  Serial.printf("URL composed is :\n%s\n",url);

  HTTPClient http;
  http.begin(url);

  int httpCode=http.GET();

  if (httpCode>0)
  {
    String payload=http.getString();
    Serial.printf("HTTP code: %d\nContent: %s\n",httpCode,payload.c_str());
    bool success=(httpCode==200);
    printf("Successful send? %d\n",(uint8_t)success);
    return success;
  } else {
    Serial.println("Error trying to send the message...");
    return false;

  }






}

bool is_usb_connected()
{
  return (digitalRead(PIN_USB_CONNECTED)==USB_CONNECTED);
}

void set_message_text(char * target,uint8_t reset_reason_code,float bat_volt,bool usb_connected)
{
  // Assembles into the target a json representation of the status after boot to be sent over wifi
  char * ptr=target;
  char temp[20];
  strcpy(ptr,"{\"reset_reason\":\"");

  strncat(ptr,reset_reasons+53*reset_reason_code,53);
  strcat(ptr,"\",\"battery_voltage\":");
  sprintf(temp,"%.3f",bat_volt);
  strcat(ptr,temp);
  strcat(ptr,",\"usb_connected\":");
  strcat(ptr,usb_connected?"true":"false");
  strcat(ptr,"}");

  Serial.printf("Assembled message for reset code %d of %d bytes:\n%s\n",reset_reason_code,strlen(ptr),ptr);

}

void sleep_esp(uint32_t duration_s)
{
  // Shut stuff down
  tp.DotStar_SetPower(false); // Power down the ws2812

  // Set timed wakeup

  esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_OFF);

  // Go to sleep
   
  Serial.println("Going to sleep");
  delay(400);
  esp_deep_sleep(duration_s * uS_TO_S_FACTOR);
  


}

bool connect_to_wifi()
{
  // returns true if successful
    
  WiFi.setHostname(hostname);
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  delay(500);
  WiFi.begin(wifi_ssid, wifi_password);
  delay(1000);
  Serial.printf("Connecting to: %s\n",wifi_ssid);
  Serial.println("");

  uint32_t wifi_timeout=millis()+WIFI_CONNECT_TIMEOUT;

  bool connecting_led_on=true;

  // Wait for connection
  while (true)
  {
    if (WiFi.status() == WL_CONNECTED)
    {
      Serial.println("\nWifi now connected");
      break;
    }
    if (millis()>wifi_timeout)
    {
      Serial.println("Pointless message saying we are restarting to have another go at connecting");
      return false;
    }
    if (connecting_led_on)
    {
      tp.DotStar_SetPixelColor(WIFI_CONNECTING_PIXEL_COLOUR);
    } else {
      tp.DotStar_Clear();
    }
    delay(500);
    Serial.print(".");
    connecting_led_on=!connecting_led_on;
  }

  Serial.println("");
  Serial.print("Connected to ");
  Serial.println(wifi_ssid);
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  return true;

}




void setup()
{
  Serial.begin(115200);
  uint32_t start_time=millis();
  tp.DotStar_SetPixelColor(BOOT_PIXEL_COLOR);
  delay(500);

  // Test pin 9




  Serial.println("booted");

  // Get reset reason
  uint8_t reset_reason=esp_reset_reason(); // Save the reset reason
  bool reset_pressed;
  if (reset_reason==8)
  {
    Serial.println("Reset due to timer, all is well");
    reset_pressed=false;
  } else {
    Serial.println("Reset due to alarm or other problem");
    reset_pressed=true;
  }
  set_message_text(message_text_json,reset_reason,tp.GetBatteryVoltage(),is_usb_connected());
  Serial.printf(message_text_json);

  // Connect to wifi
  if (!connect_to_wifi())
  {
    tp.DotStar_SetPixelColor(NO_WIFI_PIXEL_COLOR);
    delay(3000);
    Serial.println("Couldn't connect to wifi, going back to sleep now!");
    sleep_esp(SLEEP_AFTER_NO_WIFI_S); // Goes back to sleep, but for shorter time
  }

  // If we connected too fast, pause so it's 5 seconds at least
  tp.DotStar_SetPixelColor(WIFI_CONNECTED_FAST_PIXEL_COLOR);
  while (millis()<start_time+5000)
  {
    delay(100);
  }

  // Send status message
  bool send_success;
  uint8_t tries_left=5;
  while (tries_left>0)
  {
    send_success=send_status_message(message_text_json);
    if (!reset_pressed) break; // Only try once with normal messages
    if (send_success) break; // don't retry if it works
    if (WiFi.status()!=WL_CONNECTED) connect_to_wifi();
    tries_left--;
  }


  // Indicate with LED the outcomes and then sleep

  if (send_success && !reset_pressed)
  {
      // normal situation
      Serial.println("Normal update sent OK");
      tp.DotStar_SetPixelColor(NORMAL_WAKE_SENT_OK_PIXEL_COLOR);
      delay(200);
      sleep_esp(NORMAL_SLEEP_S);
  }

  if (send_success && reset_pressed)
  {
    // Help is on the way blues display
    Serial.println("blue lights...");
    for (uint16_t i=0;i<30;i++)
    {
      tp.DotStar_SetPixelColor(HELP_ON_WAY_COLOUR);
      delay(200);
      tp.DotStar_Clear();
      delay(300);
    }
    sleep_esp(NORMAL_SLEEP_S); // sleep- button can be pressed again to resent
  }

  // If there was no send success by this point it's just not going to work!

  sleep_esp(NORMAL_SLEEP_S);

  





  
}


void loop()
{
  // Never used!

  // Cycle the DotStar colour every 25 milliseconds
  tp.DotStar_CycleColor(25);

  // You can set the DotStar colour directly using r,g,b values
  // tp.DotStar_SetPixelColor( 255, 128, 0 );

  // You can set the DotStar colour directly using a uint32_t value
  // tp.DotStar_SetPixelColor( 0xFFC900 );

  // You can clear the DotStar too
  // tp.DotStar_Clear();

  // To power down the DotStar for deep sleep you call this
  // tp.DotStar_SetPower( false );
}
