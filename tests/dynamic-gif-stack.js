var GifLib = require('../build/Release/gif');
var fs = require('fs');
var Buffer = require('buffer').Buffer;

var gifStack = new GifLib.DynamicGifStack('rgba');
//gifStack.setTransparencyColor(0x00, 0x00, 0x00);

function rectDim(fileName) {
    var m = fileName.match(/^\d+-rgba-(\d+)-(\d+)-(\d+)-(\d+).dat$/);
    var dim = [m[1], m[2], m[3], m[4]].map(function (n) {
        return parseInt(n, 10);
    });
    return { x: dim[0], y: dim[1], w: dim[2], h: dim[3] }
}

var files = fs.readdirSync('./push-data');

files.forEach(function(file) {
    var dim = rectDim(file);
    var rgba = fs.readFileSync('./push-data/' + file);
    gifStack.push(rgba, dim.x, dim.y, dim.w, dim.h);
});

fs.writeFileSync('dynamic.gif', gifStack.encodeSync().toString('binary'), 'binary');

var dims = gifStack.dimensions();

console.log("GIF located at (" + dims.x + "," + dims.y + ") with width " +
    dims.width + " and height " + dims.height);

