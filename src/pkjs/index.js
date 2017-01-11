var battery_percentage = -1;
var battery_charging = -1;

navigator.getBattery().then(function(battery) {
  battery.addEventListener('chargingchange', function(){
    updateChargeInfo();
  });
  function updateChargeInfo(){
    battery_charging = battery.charging ? 1 : 0;
    console.log("Battery charging " + battery_charging);
    Pebble.sendAppMessage({'PhoneBatteryCharging' : battery_charging});
  }

  battery.addEventListener('levelchange', function(){
    updateLevelInfo();
  });
  function updateLevelInfo(){
    battery_percentage = Math.floor(battery.level * 100);
    console.log("Battery percentage " + battery_percentage);
    Pebble.sendAppMessage({'PhoneBattery' : battery_percentage});
    if (battery_charging === 0) {
      if (battery_percentage == 50) {
        Pebble.showSimpleNotificationOnPebble("Phone Battery", "Phone battery is at half");
      } else if (battery_percentage == 15) {
        Pebble.showSimpleNotificationOnPebble("Phone Battery", "Phone battery is getting low");
      }
    }
  }

  updateChargeInfo();
  updateLevelInfo();

});

Pebble.addEventListener('ready', function (e) {
  console.log('connect!' + e.ready);
  console.log(e.type);
});