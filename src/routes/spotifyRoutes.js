const express = require('express');
const router = express.Router();
const axios = require('axios');
const querystring = require('querystring');

// Spotify credentials and redirect URI (update with your Render domain)
const clientID = process.env.CLIENT_ID;
const clientSecret = process.env.CLIENT_SECRET;
const redirectURI = "https://spotify-player-esp32.onrender.com/callback";

// In-memory token storage
let tokenData = {
  access_token: null,
  refresh_token: null,
};

// Endpoint to redirect user to Spotify's OAuth page
router.get('/login', (req, res) => {
  const scope = 'user-read-playback-state user-read-currently-playing user-modify-playback-state';
  const authUrl = 'https://accounts.spotify.com/authorize?' +
    querystring.stringify({
      response_type: 'code',
      client_id: clientID,
      scope: scope,
      redirect_uri: redirectURI
    });
  res.redirect(authUrl);
});

// OAuth callback endpoint: Exchange code for tokens, broadcast them, then send success page
router.get('/callback', async (req, res) => {
  const code = req.query.code || null;
  if (!code) {
    return res.status(400).send('Error: No authorization code provided.');
  }

  const tokenUrl = 'https://accounts.spotify.com/api/token';
  const data = {
    code: code,
    redirect_uri: redirectURI,
    grant_type: 'authorization_code'
  };

  const headers = {
    'Authorization': 'Basic ' + Buffer.from(clientID + ':' + clientSecret).toString('base64'),
    'Content-Type': 'application/x-www-form-urlencoded'
  };

  try {
    const response = await axios.post(tokenUrl, querystring.stringify(data), { headers: headers });
    tokenData.access_token = response.data.access_token;
    tokenData.refresh_token = response.data.refresh_token;
    console.log("Tokens generated:", tokenData);

    // Broadcast token data to all connected WebSocket clients
    wss.clients.forEach((client) => {
      if (client.readyState === WebSocket.OPEN) {
        client.send(JSON.stringify(tokenData));
      }
    });

    // Send the success page to the user
    res.sendFile(path.join(__dirname, 'public', 'success.html'));
  } catch (error) {
    console.error("Error exchanging code for tokens:", error);
    res.status(500).send('Error retrieving tokens.');
  }
});

// Refresh token endpoint (if needed)
router.get('/refresh', async (req, res) => {
  const refreshToken = req.query.refresh_token;
  if (!refreshToken) {
    return res.status(400).send('Missing refresh token.');
  }

  const tokenUrl = 'https://accounts.spotify.com/api/token';
  const data = {
    grant_type: 'refresh_token',
    refresh_token: refreshToken
  };

  const headers = {
    'Authorization': 'Basic ' + Buffer.from(clientID + ':' + clientSecret).toString('base64'),
    'Content-Type': 'application/x-www-form-urlencoded'
  };

  try {
    const response = await axios.post(tokenUrl, querystring.stringify(data), { headers: headers });
    res.json({
      access_token: response.data.access_token,
      expires_in: response.data.expires_in
    });
  } catch (error) {
    console.error("Error refreshing token:", error);
    res.status(500).send('Error refreshing token.');
  }
});

module.exports = router;
