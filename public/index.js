document.addEventListener("DOMContentLoaded", function() {
    const authUrl = "https://accounts.spotify.com/authorize?client_id=d831b0f307a1448b8ba4aa26c71f1496&response_type=code&redirect_uri=https://spotify-player-esp32.onrender.com/spotify/callback&scope=user-read-playback-state%20user-read-currently-playing%20user-modify-playback-state";
    
    new QRCode(document.getElementById("qrcode"), {
      text: authUrl,
      width: 256,
      height: 256,
      colorDark : "#000000",
      colorLight : "#ffffff",
      correctLevel : QRCode.CorrectLevel.H
    });
  });
  