require('dotenv').config();
const express = require('express');
const app = express();
const port = process.env.PORT || 3000;
const path = require('path');

app.use(express.urlencoded({ extended: true })); // For form data
app.use(express.json()); // For JSON payloads

app.use(express.static(path.join(__dirname, 'public')));

const dataRoutes = require('./src/routes/dataRoutes.js');
app.use('/data', dataRoutes);

app.listen(port, () => {
  console.log(`Server is running on port ${port}`);
});
