const pool = require('../services/db.js'); // Your database connection

module.exports.registerUser = async (data, callback) =>
    {
        const SQLSTATEMENT = `
        INSERT INTO "Users" (id)
        VALUES ($1)
        ON CONFLICT (id) DO NOTHING;
        `;

        const VALUES = [data.id];
        pool.query(SQLSTATEMENT, VALUES, callback);
    };

module.exports.checkRefreshToken = async (data, callback) => 
    {
        const SQLSTATEMENT = `
            SELECT refresh_token 
            FROM Users 
            WHERE id = $1; 
        `;
    
        const VALUES = [data.id];
    
        try {
            const result = await pool.query(SQLSTATEMENT, VALUES);
            callback(null, result);
        } catch (error) {
            callback(error);
        }
    };
    

module.exports = {
  registerUser,checkRefreshToken 
};
