#include <Arduino.h>
#include <TinyPICO.h>

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


struct StatusMessage_t {
  char reset_reason[51];
  float battery_voltage;
  bool is_charging;
};

char message_text_json[200];
// e.g. {"reset_reason":"01234567890123456789012345678901234567890123456789","battery_voltage":3.215,"is_charging":false}



void set_message_text(char * target,uint8_t reset_reason_code,float bat_volt,bool chrging)
{
  // Assembles into the target a json representation of the status after boot to be sent over wifi
  char * ptr=target;
  char temp[20];
  strcpy(ptr,"{\"reset_reason\":\"");

  strncat(ptr,reset_reasons+53*reset_reason_code,53);
  strcat(ptr,"\",\"battery_voltage\":");
  sprintf(temp,"%.3f",bat_volt);
  strcat(ptr,temp);
  strcat(ptr,",\"is_charging\":");
  strcat(ptr,chrging?"true":"false");
  strcat(ptr,"}");

  Serial.printf("Assembled message for reset code %d of %d bytes:\n%s\n",reset_reason_code,strlen(ptr),ptr);

}


void setup()
{
  Serial.begin(115200);
  delay(500);
  Serial.println("booted");
  // Get reset reason
  uint8_t reset_reason=esp_reset_reason(); // Save the reset reason

  for (uint8_t i=10;i>0;i--)
  {
    Serial.println(i);
    delay(500);
  }


  // Make the status message
  set_message_text(message_text_json,reset_reason,tp.GetBatteryVoltage(),tp.IsChargingBattery());
  Serial.printf(message_text_json);

  


  // Send status message


  // Indicate with LED the outcome
  


  // Set timed wakeup

  esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_OFF);
  // Switch stuff off


  // Go to sleep

    
  Serial.println("Going to sleep");
  esp_deep_sleep(5 * uS_TO_S_FACTOR);

  
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
