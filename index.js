var gif = require('bindings')('gif'),
    fs = require('fs');

module.exports = gif;

/*module.exports.AsyncAnimatedGif.setTmpDir = function setTmpDir(path) {
    if (typeof path !== 'string') {
      throw Error('Argument \'path\' should be a string.');
    }

    var stats = fs.statSync(path);

    if (!stats.IsDirectory()) {
      throw Error(path + ' is not a directory.');
    }

    gif.setTmpDir(path);
}*/
