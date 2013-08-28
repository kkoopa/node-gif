var fs  = require('fs');
var Gif = require('../build/Release/gif').Gif;
var Buffer = require('buffer').Buffer;

var terminal = fs.readFileSync('./terminal.rgba');

var gif = new Gif(terminal, 720, 400, 'rgba').encodeSync();

fs.writeFileSync('./terminal.gif', gif.toString('binary'), 'binary');

