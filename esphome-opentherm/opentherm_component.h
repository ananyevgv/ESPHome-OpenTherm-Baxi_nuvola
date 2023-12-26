#include "esphome.h"
#include "esphome/components/sensor/sensor.h"
#include "OpenTherm.h"
#include "opentherm_switch.h"
#include "opentherm_climate.h"
#include "opentherm_binary.h"
#include "opentherm_output.h"

// Pins to OpenTherm Adapter
int inPin = D2; 
int outPin = D1;
OpenTherm ot(inPin, outPin, false);

IRAM_ATTR void handleInterrupt() {
	ot.handleInterrupt();
}

class OpenthermComponent: public PollingComponent {
private:
  const char *TAG = "opentherm_component";
  OpenthermFloatOutput *pid_output_; 
public:
  Switch *thermostatSwitch = new OpenthermSwitch();
  Sensor *external_temperature_sensor = new Sensor();
  Sensor *boiler_temperature = new Sensor();
  Sensor *modulation_sensor = new Sensor();
  Sensor *heating_target_temperature_sensor = new Sensor();
  Sensor *Efault_sensor = new Sensor(); 
  OpenthermClimate *hotWaterClimate = new OpenthermClimate();
  OpenthermClimate *heatingWaterClimate = new OpenthermClimate();
  BinarySensor *flame = new OpenthermBinarySensor();
  BinarySensor *diagnostic = new OpenthermBinarySensor();
  BinarySensor *fault = new OpenthermBinarySensor(); 
  
  // Set 3 sec. to give time to read all sensors (and not appear in HA as not available)
  OpenthermComponent(): PollingComponent(5000) {
  }

  void set_pid_output(OpenthermFloatOutput *pid_output) { pid_output_ = pid_output; }


  void setup() override {
    // This will be called once to set up the component
    // think of it as the setup() call in Arduino
      ESP_LOGD("opentherm_component", "Setup");
	  
      unsigned long request3 = ot.buildRequest(OpenThermRequestType::READ, OpenThermMessageID::SConfigSMemberIDcode, 0xFFFF);
      unsigned long respons3 = ot.sendRequest(request3);
      uint8_t SlaveMemberIDcode = respons3 >> 0 & 0xFF;
      uint8_t flags = (respons3 & 0xFFFF) >> 8 & 0xFF;

      ESP_LOGD ("opentherm_component", "SConfigSMemberIDcode %X", request3 );

      unsigned long request2 = ot.buildRequest(OpenThermRequestType::WRITE, OpenThermMessageID::MConfigMMemberIDcode, SlaveMemberIDcode);
      unsigned long respons2 = ot.sendRequest(request2);
      ESP_LOGD ("opentherm_component", "MConfigMMemberIDcode %X",  request2 ); 

      unsigned long request127 = ot.buildRequest(OpenThermRequestType::READ, OpenThermMessageID::SlaveVersion, 0);
      ESP_LOGD ("opentherm_component", "SlaveVersion %X",  request127 ); 

      unsigned long request126 = ot.buildRequest(OpenThermRequestType::WRITE, OpenThermMessageID::MasterVersion, 0x013F);
      unsigned long respons126 = ot.sendRequest(request126);
      ESP_LOGD ("opentherm_component", "MasterVersion %X",  request126 ); 

	  
      ot.begin(handleInterrupt);

      thermostatSwitch->add_on_state_callback([=](bool state) -> void {
        ESP_LOGD ("opentherm_component", "termostatSwitch_on_state_callback %d", state);    
      });

      // Adjust HeatingWaterClimate depending on PID
      // heatingWaterClimate->set_supports_heat_cool_mode(this->pid_output_ != nullptr);
      heatingWaterClimate->set_supports_two_point_target_temperature(this->pid_output_ != nullptr);

      hotWaterClimate->set_temperature_settings(5, 6, 0);
      heatingWaterClimate->set_temperature_settings(0, 0, 30);
      hotWaterClimate->setup();
      heatingWaterClimate->setup();
  }
  
 
  
  
  float getExternalTemperature() {
      unsigned long response = ot.sendRequest(ot.buildRequest(OpenThermRequestType::READ, OpenThermMessageID::Toutside, 0));
      return ot.isValidResponse(response) ? ot.getFloat(response) : -1;
  }

  
  
  float getHotWaterTemperature() {
      unsigned long response = ot.sendRequest(ot.buildRequest(OpenThermRequestType::READ, OpenThermMessageID::Tdhw, 0));
      return ot.isValidResponse(response) ? ot.getFloat(response) : -1;
  }

  bool setHotWaterTemperature(float temperature) {
      unsigned int data = ot.temperatureToData(temperature);
      unsigned long request = ot.buildRequest(OpenThermRequestType::WRITE, OpenThermMessageID::TdhwSet, data);
      unsigned long response = ot.sendRequest(request);
      return ot.isValidResponse(response);
  }




  void update() override {

    
    ESP_LOGD("opentherm_component", "update heatingWaterClimate: %i", heatingWaterClimate->mode);
    ESP_LOGD("opentherm_component", "update hotWaterClimate: %i", hotWaterClimate->mode);
    
    bool enableCentralHeating = heatingWaterClimate->mode == ClimateMode::CLIMATE_MODE_HEAT;
    bool enableHotWater = hotWaterClimate->mode == ClimateMode::CLIMATE_MODE_HEAT;
    bool enableCentralHeating2 = hotWaterClimate->mode == ClimateMode::CLIMATE_MODE_HEAT;
    bool enableCooling = false; // this boiler is for heating only

    
    //Set/Get Boiler Status
    auto response = ot.setBoilerStatus(enableCentralHeating, enableHotWater, enableCooling);
    bool isFlameOn = ot.isFlameOn(response);
    bool isDiagnostic = ot.isDiagnostic(response);
    bool isFault = ot.isFault(response);
    bool isCentralHeatingActive = ot.isCentralHeatingActive(response);
    bool isHotWaterActive = ot.isHotWaterActive(response);
    bool isCentralHeating2 = ot.isCentralHeating2Active(response);
    

    float hotWater_temperature = getHotWaterTemperature();


    // Set temperature depending on room thermostat
    float heating_target_temperature;
    if (this->pid_output_ != nullptr) {
      float pid_output = pid_output_->get_state();
      if (pid_output == 0.0f) {
        heating_target_temperature = 0.0f;
      }
      else {
        heating_target_temperature =  pid_output * (heatingWaterClimate->target_temperature_high - heatingWaterClimate->target_temperature_low) 
        + heatingWaterClimate->target_temperature_low;      
      }
      ESP_LOGD("opentherm_component", "setBoilerTemperature  at %f °C (from PID Output)", heating_target_temperature);
    }
    else if (thermostatSwitch->state) {
      heating_target_temperature = heatingWaterClimate->target_temperature;
      ESP_LOGD("opentherm_component", "setBoilerTemperature  at %f °C (from heating water climate)", heating_target_temperature);
    }
    else {
      heating_target_temperature = 0.0;
      ESP_LOGD("opentherm_component", "setBoilerTemperature at %f °C (default low value)", heating_target_temperature);
    }
    ot.setBoilerTemperature(heating_target_temperature);

    // Set hot water temperature
    setHotWaterTemperature(hotWaterClimate->target_temperature);

    float boilerTemperature = ot.getBoilerTemperature();
    float ext_temperature = getExternalTemperature();
    float Efault =  ot.getEFault();
    float modulation = ot.getModulation();

    // Publish sensor values
    flame->publish_state(isFlameOn);
    diagnostic->publish_state(isDiagnostic);
    fault->publish_state(isFault); 
    external_temperature_sensor->publish_state(ext_temperature);
    boiler_temperature->publish_state(boilerTemperature);
    Efault_sensor->publish_state(Efault);
    modulation_sensor->publish_state(modulation);
    
    heating_target_temperature_sensor->publish_state(heating_target_temperature);

    // Publish status of thermostat that controls hot water
    hotWaterClimate->current_temperature = hotWater_temperature;
    hotWaterClimate->action = isHotWaterActive ? ClimateAction::CLIMATE_ACTION_HEATING : ClimateAction::CLIMATE_ACTION_OFF;
    hotWaterClimate->publish_state();
    
    // Publish status of thermostat that controls heating
    heatingWaterClimate->current_temperature = boilerTemperature;
    heatingWaterClimate->action = isCentralHeatingActive && isFlameOn ? ClimateAction::CLIMATE_ACTION_HEATING : ClimateAction::CLIMATE_ACTION_OFF;
    heatingWaterClimate->publish_state();
  }

};
