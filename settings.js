// Node-RED settings for the SecureDrop gateway.
// Start with:  npm start      (or: npx node-red -u . -s settings.js)
module.exports = {
    flowFile: "flows.json",
    uiPort: process.env.PORT || 1880,
    // Relative paths in file nodes (archive/...) resolve next to this file.
    fileWorkingDirectory: __dirname,
    // Keep the delivery list and counter across restarts.
    contextStorage: { default: { module: "localfilesystem" } },
    // Largest body accepted from the simulator page.
    apiMaxLength: "10mb",
    logging: { console: { level: "info", metrics: false, audit: false } }
};
