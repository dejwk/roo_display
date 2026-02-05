(function () {
  var script = document.createElement('script');
  script.src = 'https://unpkg.com/doxygen-awesome-css@2.4.1/doxygen-awesome-darkmode-toggle.js';
  script.onload = function () {
    if (!window.DoxygenAwesomeDarkModeToggle) {
      return;
    }
    var prefersDarkKey = DoxygenAwesomeDarkModeToggle.prefersDarkModeInLightModeKey;
    var prefersLightKey = DoxygenAwesomeDarkModeToggle.prefersLightModeInDarkModeKey;
    var hasPreference = !!(localStorage.getItem(prefersDarkKey) || localStorage.getItem(prefersLightKey));
    if (!hasPreference) {
      DoxygenAwesomeDarkModeToggle.userPreference = true;
    }
    DoxygenAwesomeDarkModeToggle.init();
  };
  document.head.appendChild(script);
})();
