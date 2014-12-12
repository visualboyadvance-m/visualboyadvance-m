chrome.app.runtime.onLaunched.addListener(function() {
  chrome.app.window.create('game.html', {
    bounds: {
      width: 480,
      height: 320
    },
    resizable: true,
    minWidth: 480,
    minHeight: 320
  });
});
