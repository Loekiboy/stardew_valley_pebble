Pebble.addEventListener('ready', function(e) {
  getWeather();
});

Pebble.addEventListener('appmessage', function(e) {
  getWeather();
});

// WMO weather codes van Open-Meteo -> onze icoon-id's
// 0 = Zon, 1 = Regen, 2 = Sneeuw, 3 = Onweer, 4 = Wind
function mapWeatherCode(code, windSpeed) {
  if (code >= 95) return 3;                                                  // onweer
  if ((code >= 71 && code <= 77) || (code >= 85 && code <= 86)) return 2;    // sneeuw
  if ((code >= 51 && code <= 67) || (code >= 80 && code <= 82)) return 1;    // regen / motregen
  if (windSpeed >= 40) return 4;                                             // harde wind
  return 0;                                                                  // helder / bewolkt
}

function sendWeather(pebbleWeatherId) {
  Pebble.sendAppMessage(
    {'WEATHER_KEY': pebbleWeatherId},
    function() {
      console.log('Weer verzonden: ' + pebbleWeatherId);
    },
    function() {
      console.log('Verzenden van weer mislukt');
    }
  );
}

function getWeather() {
  navigator.geolocation.getCurrentPosition(
    function(position) {
      var lat = position.coords.latitude;
      var lon = position.coords.longitude;

      var url = 'https://api.open-meteo.com/v1/forecast?latitude=' + lat +
                '&longitude=' + lon + '&current_weather=true';

      var xhr = new XMLHttpRequest();
      xhr.onload = function() {
        if (xhr.status < 200 || xhr.status >= 300) {
          console.log('Weer API fout: HTTP ' + xhr.status);
          return;
        }

        var json;
        try {
          json = JSON.parse(this.responseText);
        } catch (e) {
          console.log('Kon weerdata niet lezen');
          return;
        }

        if (!json.current_weather) {
          console.log('Geen current_weather in antwoord');
          return;
        }

        var weatherCode = json.current_weather.weathercode;
        var windSpeed = json.current_weather.windspeed;
        sendWeather(mapWeatherCode(weatherCode, windSpeed));
      };
      xhr.onerror = function() {
        console.log('Netwerkfout bij ophalen weer');
      };
      xhr.open('GET', url);
      xhr.send();
    },
    function(err) {
      console.log('Fout bij ophalen locatie');
    },
    {timeout: 15000, maximumAge: 60000}
  );
}
