require('dotenv').config();
const express = require('express');
const app = express();
const port = process.env.PORT || 3000;
const path = require('path');

app.use(express.urlencoded({ extended: true })); // For form data
app.use(express.json()); // For JSON payloads

app.use(express.static(path.join(__dirname, 'public')));

app.get('/', (req, res) => {
  res.sendFile(path.join(__dirname, 'public', 'index.html'));
});

app.get('/form', (req, res) => {
  res.sendFile(path.join(__dirname, 'public', 'form.html'));
});

app.get('/spotify/callback', (req, res) => {
  res.sendFile(path.join(__dirname, 'public', 'success.html'));
});

const dataRoutes = require('./src/routes/dataRoutes.js');
app.use('/data', dataRoutes);
const SpotifyRoutes = require('./src/routes/spotifyRoutes.js');
app.use('/spotify', SpotifyRoutes);

app.listen(port, () => {
  console.log(`Server is running on port ${port}`);
});
