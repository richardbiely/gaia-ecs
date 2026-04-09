(function() {
  var scriptRoot = (function() {
    var script = document.currentScript;
    if (!script || !script.src) {
      return null;
    }

    return new URL("./", script.src);
  })();

  function scriptRootUrl() {
    return scriptRoot;
  }

  function currentVersionContext(siteRoot, payload) {
    var relative = window.location.href.slice(siteRoot.href.length);
    relative = relative.split("#")[0].split("?")[0];

    if (!relative) {
      relative = "index.html";
    }

    var parts = relative.split("/");
    if (parts.length === 0) {
      return null;
    }

    var versionPath = parts[0];
    if (versionPath !== "latest" && !/^v\d/.test(versionPath)) {
      if (payload && payload.latest && (payload.latest.path === "./" || payload.latest.path === "")) {
        return {
          versionPath: "",
          pageSuffix: relative,
          rootMode: true
        };
      }

      return null;
    }

    return {
      versionPath: versionPath,
      pageSuffix: parts.slice(1).join("/")
    };
  }

  function targetHost() {
    var toolbar = document.getElementById("gaia-doc-toolbar");
    if (toolbar) {
      var toolbarHost = document.getElementById("gaia-version-selector-toolbar-host");
      if (!toolbarHost) {
        toolbarHost = document.createElement("div");
        toolbarHost.id = "gaia-version-selector-toolbar-host";
        toolbarHost.className = "gaia-version-selector-host";
        toolbar.appendChild(toolbarHost);
      }

      return toolbarHost;
    }

    var projectNumber = document.getElementById("projectnumber");
    if (projectNumber) {
      projectNumber.classList.add("gaia-version-selector-host");
      return projectNumber;
    }

    var projectName = document.getElementById("projectname");
    if (!projectName || !projectName.parentElement) {
      return null;
    }

    var host = document.createElement("div");
    host.id = "projectnumber";
    host.className = "gaia-version-selector-host";
    projectName.parentElement.appendChild(host);
    return host;
  }

  function optionData(payload) {
    var items = [];

    if (payload.latest) {
      items.push(payload.latest);
    }

    if (Array.isArray(payload.versions)) {
      items = items.concat(payload.versions);
    }

    return items;
  }

  function inlinePayload() {
    if (typeof window === "undefined") {
      return null;
    }

    return window.GAIA_VERSION_SELECTOR_DATA || null;
  }

  function isSelected(entry, payload, context) {
    if (!context) {
      return false;
    }

    if (context.rootMode) {
      return entry.path === "./" || entry.path === "";
    }

    if (context.versionPath + "/" === entry.path) {
      return true;
    }

    return !!(payload.latest && entry.path === payload.latest.path && payload.latest.version_path === context.versionPath + "/");
  }

  function samePageUrl(siteRoot, entry, context) {
    var suffix = context && context.pageSuffix ? context.pageSuffix : "index.html";
    return new URL(entry.path + suffix, siteRoot);
  }


  function mountSelector(payload, siteRoot, context) {
    var host = targetHost();
    if (!host) {
      return;
    }

    var entries = optionData(payload);
    if (entries.length === 0) {
      return;
    }

    var wrapper = document.createElement("div");
    wrapper.className = "gaia-version-selector";

    var select = document.createElement("select");
    select.setAttribute("aria-label", "Documentation version");

    entries.forEach(function(entry) {
      var option = document.createElement("option");
      option.value = entry.path;
      option.textContent = entry.label;
      if (isSelected(entry, payload, context)) {
        option.selected = true;
      }
      select.appendChild(option);
    });

    select.addEventListener("change", function() {
      var selected = entries.find(function(entry) {
        return entry.path === select.value;
      });

      if (!selected) {
        return;
      }

      window.location.href = samePageUrl(siteRoot, selected, context).href;
    });

    wrapper.appendChild(select);
    host.replaceChildren(wrapper);
  }

  function loadSelector() {
    var siteRoot = scriptRootUrl();
    if (!siteRoot) {
      return;
    }

    var payload = inlinePayload();
    var useInlineOnly = window.location.protocol === "file:" && !!payload;
    if (useInlineOnly) {
      var inlineContext = currentVersionContext(siteRoot, payload);
      if (!inlineContext) {
        return;
      }

      mountSelector(payload, siteRoot, inlineContext);
      return;
    }

    fetch(new URL("versions.json", siteRoot), { cache: "no-store" })
      .then(function(response) {
        if (!response.ok) {
          throw new Error("HTTP " + response.status);
        }
        return response.json();
      })
      .then(function(payload) {
        var context = currentVersionContext(siteRoot, payload);
        if (!context) {
          return;
        }

        mountSelector(payload, siteRoot, context);
      })
      .catch(function() {
        if (!payload) {
          return;
        }

        var fallbackContext = currentVersionContext(siteRoot, payload);
        if (!fallbackContext) {
          return;
        }

        mountSelector(payload, siteRoot, fallbackContext);
      });
  }

  if (document.readyState === "loading") {
    document.addEventListener("DOMContentLoaded", loadSelector, { once: true });
  } else {
    loadSelector();
  }
})();
