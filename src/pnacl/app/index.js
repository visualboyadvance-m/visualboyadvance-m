var plugin = document.getElementById('naclModule');
var firstLoad = false;
var naclVisible = false;
var localfs = null;
var syncfs = null;
var saveDirEntry = null;
var gamepadCheckbox = document.getElementById('gamepadCheckbox');

var postFileCallback = function() {};

function setFilenameErrorVisible(visible) {
  document.getElementById('filenameErrorMessage').style.display =
      visible ? 'block' : 'none';
}

function setHelptextsVisible(visible) {
  var helptexts = document.getElementsByClassName('helptext');
  for (var i = 0; i < helptexts.length; i++) {
    helptexts[i].style.display = visible ? 'block' : 'none';
  }
}

function setNaclVisible(visible) {
  naclVisible = visible;
  document.getElementsByTagName('body')[0].className =
      visible ? 'lightsOff' : 'lightsOn';
  setHelptextsVisible(!visible);
  scaleNacl();
}

function scaleNacl() {
  var bounds = chrome.app.window.current().getBounds();
  var scaleX = bounds.width / plugin.width;
  var scaleY = bounds.height / plugin.height;
  var scale = naclVisible ? Math.min(scaleX, scaleY) : 0;
  plugin.style.webkitTransform = 'scale(' + scale + ')';
}

function makeNewPlugin() {
  var newNode = plugin.cloneNode(true);
  plugin.parentNode.appendChild(newNode);
  plugin.parentNode.removeChild(plugin);
  plugin = document.getElementById('naclModule');
  setNaclVisible(false);
}

function showOpenFileDialog() {
  setFilenameErrorVisible(false);
  chrome.fileSystem.chooseEntry({
      'type': 'openFile',
      'accepts': [{'description': 'GBA Roms', 'extensions': ['zip', 'gba']}]
    },
    function(entry) {
      if (!entry)
        return;

      if (entry.fullPath.indexOf(' ') != -1) {
        setFilenameErrorVisible(true);
        return;
      }

      postFileCallback = function() {
        setNaclVisible(true);
        plugin.postMessage({
          'path': entry.fullPath,
          'filesystem': entry.filesystem,
          'gamepad' : gamepadCheckbox.checked
        });
        postFileCallback = function() {};
      };
      makeNewPlugin();
    });
}

function copyToSyncFileSystem() {
  if (!syncfs || !localfs) {
    setTimeout(copyToSyncFileSystem, 1000);
    return;
  }

  reader = saveDirEntry.createReader();
  var copyIfNewer = function(entries) {
    if (entries.length == 0)
      return;

    for (i in entries) {
      var entry = entries[i];
      if (!entry.isFile)
        return;
      (function() {
        var e = entry;
        var doCopy = function() {
          console.log('Upsyncing ' + e.name);
          e.copyTo(syncfs.root);
        };
        e.getMetadata(function(localMeta) {
          syncfs.root.getFile(e.name, {}, function(syncEntry) {
            syncEntry.getMetadata(function(syncMeta) {
//              syncEntry.remove(function() {
                if (syncMeta.modificationTime < localMeta.modificationTime)
                  doCopy();
//              });
            }, doCopy);
          }, doCopy);
        });
      })();
    }
    reader.readEntries(copyIfNewer);
  };
  reader.readEntries(copyIfNewer);

  setTimeout(copyToSyncFileSystem, 30000);
}

function initSyncFileSystem() {
  syncfs = null;
  chrome.syncFileSystem.getServiceStatus(function(status) {
    if (status == 'running') {
      chrome.syncFileSystem.requestFileSystem(function(fs) {
        syncfs = fs;
      });
    } else {
      setTimeout(initSyncFileSystem, 5000);
    }
  });
}

window.webkitRequestFileSystem(window.PERSISTENT, 0, function(fs) {
  localfs = fs;
  fs.root.getDirectory(
      'states', { create: true }, function(d) { saveDirEntry = d; });
});

initSyncFileSystem();
copyToSyncFileSystem();
chrome.syncFileSystem.onServiceStatusChanged.addListener(initSyncFileSystem);
chrome.syncFileSystem.onFileStatusChanged.addListener(function(detail) {
  /*
  if (saveDirEntry && detail.status == 'synced' &&
      detail.direction == 'remote_to_local') {
    if (detail.action == 'deleted') {
      saveDirEntry.getFile(detail.fileEntry.name, {}, function(entry) {
        console.log('Deleting' + detail.fileEntry.name);
        entry.remove(function() {});
      });
      detail.fileEntry.remove(function() {});
    } else {
      detail.fileEntry.copyTo(saveDirEntry);
      console.log('Downsyncing ' + detail.fileEntry.name);
    }
  }*/
});

var is_cros = window.navigator.userAgent.toLowerCase().indexOf('cros') != -1;
document.getElementById('normalSaveStateKeys').style.display = is_cros ? 'none' : 'block';
document.getElementById('crosSaveStateKeys').style.display = is_cros ? 'block' : 'none';

document.getElementById('importSaveButton').onclick = function() {
  chrome.fileSystem.chooseEntry({
      'type': 'openFile',
      'accepts': [{'description': 'VBA-M Save Files', 'extensions': ['sav', 'sgm']}]
    },
    function(entry) {
      if (!entry)
        return;

      if (entry.fullPath.indexOf(' ') != -1) {
        setFilenameErrorVisible(true);
        return;
      }

      entry.copyTo(saveDirEntry);
      console.log('Copied ' + entry.name + ' to HTML5fs');
    });
};

// Gamepad checkbox.
gamepadCheckbox.onclick = function() {
  chrome.storage.local.set({'gamepad': gamepadCheckbox.checked});
}
chrome.storage.local.get('gamepad', function(items) {
  gamepadCheckbox.checked = items['gamepad'];
});

// Advanced settings
document.getElementById('advancedButton').onclick = function() {
  document.getElementById('advancedButton').style.display = 'none';
  var advancedSettings = document.getElementsByClassName('advancedSetting');
  for (var i = 0; i < advancedSettings.length; i++)
    advancedSettings[i].style.display = 'block';
};

// Add window scaling.
chrome.app.window.current().onBoundsChanged.addListener(scaleNacl);

// NaCl plugin listener
var listener = document.getElementById('listener');
listener.addEventListener(
  'message',
  function(e) {
    console.log(e);
  }, true);

listener.addEventListener(
  'crash',
  function(e) {
    makeNewPlugin();
  }, true);

listener.addEventListener(
  'load',
  function(e) {
    setHelptextsVisible(true);
    document.getElementById('loadingMessage').style.display = 'none';
    postFileCallback();
    firstLoad = true;
    scaleNacl();
  }, true);

listener.addEventListener(
  'progress',
  function(e) {
    var percent = e.loaded / e.total;
    document.getElementById('progress').style.width = (percent * 100) + '%';
  }, true);

document.addEventListener('keydown', function(e) {
  // Ctrl + o
  if (firstLoad && e.ctrlKey && e.keyCode == 79) {
    showOpenFileDialog();
  }
  // ESC
  if (e.keyCode == 27) {
    // makeNewPlugin();
    e.preventDefault();
  }
}, true);
