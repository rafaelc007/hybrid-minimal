// ============================================================================
// Hybrid Minimal Watchface — PebbleKit JS Companion
// ============================================================================

// Map WMO weather code (Open-Meteo) to lowercase tokens matched by C icon selector
function wmoToken(code) {
  if (code === 0)                         return 'clear';       // clear sky
  if (code <= 3)                          return 'cloud';       // partly cloudy / overcast
  if (code <= 48)                         return 'cloud';       // fog, depositing rime fog
  if (code <= 55)                         return 'drizzle';     // drizzle (light/moderate/dense)
  if (code <= 57)                         return 'sleet';       // freezing drizzle
  if (code <= 65)                         return 'rain';        // rain (slight/moderate/heavy)
  if (code <= 67)                         return 'sleet';       // freezing rain
  if (code <= 77)                         return 'snow';        // snow / snow grains
  if (code <= 82)                         return 'rain';        // rain showers
  if (code <= 86)                         return 'snow';        // snow showers
  if (code <= 99)                         return 'thunder';     // thunderstorm (with/without hail)
  return 'cloud';
}

function fetchWeather() {
  navigator.geolocation.getCurrentPosition(
    function(pos) {
      var lat = pos.coords.latitude.toFixed(4);
      var lon = pos.coords.longitude.toFixed(4);
      var url = 'https://api.open-meteo.com/v1/forecast' +
                '?latitude=' + lat +
                '&longitude=' + lon +
                '&current_weather=true' +
                '&temperature_unit=celsius';
      var xhr = new XMLHttpRequest();
      xhr.onload = function() {
        try {
          var data = JSON.parse(this.responseText);
          var cw   = data.current_weather;
          var temp = Math.round(cw.temperature);
          var cond = wmoToken(cw.weathercode);
          Pebble.sendAppMessage(
            { WeatherTemp: temp, WeatherIcon: cond },
            function() { console.log('Weather sent: ' + temp + '\u00b0 ' + cond); },
            function(e) { console.log('Weather send failed: ' + JSON.stringify(e)); }
          );
        } catch (e) {
          console.log('Weather parse error: ' + e);
        }
      };
      xhr.open('GET', url);
      xhr.send();
    },
    function(err) {
      console.log('Geolocation error: ' + err.message);
    },
    { timeout: 15000 }
  );
}

Pebble.addEventListener('ready', function() {
  console.log('PebbleKit JS ready');
  fetchWeather();
});

Pebble.addEventListener('appmessage', function(e) {
  console.log('Received message from watch: ' + JSON.stringify(e.payload));
});
