const express = require('express');
const router = express.Router();
const axios = require('axios');
const querystring = require('querystring');

// Spotify credentials and redirect URI (update with your Render domain)
const clientID = process.env.CLIENT_ID;
const clientSecret = process.env.CLIENT_SECRET;
const redirectURI = "https://spotify-player-esp32.onrender.com/spotify/callback";

// Endpoint to redirect user to Spotify's OAuth page
router.get('/login', (req, res) => {
  const deviceId = req.query.id;

  if (!deviceId) {
    return res.status(400).send('Missing device ID');
  }

  const scope = 'user-read-playback-state user-read-currently-playing user-modify-playback-state';

  const authUrl = 'https://accounts.spotify.com/authorize?' + querystring.stringify({
    response_type: 'code',
    client_id: clientID,
    scope: scope,
    redirect_uri: redirectURI, // must match exactly
    state: deviceId // include UUID in state
  });
  
  res.redirect(authUrl);
});

// OAuth callback endpoint: Exchange code for tokens, broadcast them, then send success page
router.get('/callback', async (req, res) => {
  const code = req.query.code || null;
  const deviceId = req.query.state;

  if (!code || !deviceId) {
    return res.status(400).send('Missing authorization code or device ID.');
  }

  const tokenUrl = 'https://accounts.spotify.com/api/token';
  const data = {
    code,
    redirect_uri: redirectURI,
    grant_type: 'authorization_code'
  };

  const headers = {
    'Authorization': 'Basic ' + Buffer.from(`${clientID}:${clientSecret}`).toString('base64'),
    'Content-Type': 'application/x-www-form-urlencoded'
  };

  try {
    const response = await axios.post(tokenUrl, querystring.stringify(data), { headers });

    const refreshToken = response.data.refresh_token;
    const accessToken = response.data.access_token;

    console.log("Tokens generated:", { accessToken, refreshToken });

    // ✅ Save refresh token to DB
    await dataModel.updateRefreshToken({ id: deviceId, refresh_token: refreshToken });

    // Send success page
    res.sendFile(path.join(__dirname, '..', 'public', 'success.html'));
  } catch (error) {
    console.error("Error exchanging code for tokens:", error.response?.data || error.message);
    res.status(500).send('Error retrieving tokens.');
  }
});

// Refresh token endpoint (if needed)
router.post('/refresh', async (req, res) => {
  const { refresh_token, device_id } = req.body;

  if (!refresh_token || !device_id) {
    return res.status(400).json({ error: 'Missing refresh token or device ID.' });
  }

  const tokenUrl = 'https://accounts.spotify.com/api/token';
  const data = {
    grant_type: 'refresh_token',
    refresh_token: refresh_token
  };

  const headers = {
    'Authorization': 'Basic ' + Buffer.from(clientID + ':' + clientSecret).toString('base64'),
    'Content-Type': 'application/x-www-form-urlencoded'
  };

  try {
    const response = await axios.post(tokenUrl, querystring.stringify(data), { headers: headers });

    // Optional: log device activity or update last_seen column
    console.log(`Device ${device_id} refreshed token`);

    res.json({
      access_token: response.data.access_token,
      expires_in: response.data.expires_in
    });
  } catch (error) {
    console.error(`Error refreshing token for device ${device_id}:`, error);
    res.status(500).json({ error: 'Failed to refresh token.' });
  }
});


module.exports = router;
