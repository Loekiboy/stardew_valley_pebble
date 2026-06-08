Pebble.addEventListener('ready', function(e) {
  getWeather();
});

Pebble.addEventListener('appmessage', function(e) {
  getWeather();
});

function getWeather() {
  navigator.geolocation.getCurrentPosition(
    function(position) {
      var lat = position.coords.latitude;
      var lon = position.coords.longitude;
      
      var url = 'https://api.open-meteo.com/v1/forecast?latitude=' + lat + '&longitude=' + lon + '&current_weather=true';

      var xhr = new XMLHttpRequest();
      xhr.onload = function () {
        var json = JSON.parse(this.responseText);
        var weatherCode = json.current_weather.weathercode;
        
        // 0 = Sun, 1 = Rain, 2 = Snow, 3 = Thunder, 4 = Wind
        var pebbleWeatherId = 0; 
        
        if (weatherCode === 0) {
          pebbleWeatherId = 0;
        } else if (weatherCode >= 1 && weatherCode <= 65) {
          pebbleWeatherId = 1;
        } else if (weatherCode >= 71 && weatherCode <= 86) {
          pebbleWeatherId = 2;
        } else if (weatherCode >= 95) {
          pebbleWeatherId = 3;
        } else {
          pebbleWeatherId = 4;
        }

        Pebble.sendAppMessage({'WEATHER_KEY': pebbleWeatherId});
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