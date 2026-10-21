/**
 * OmniConvert - Material You UI Engine & Controller
 * Universal Offline Converter & Compressor (PDF24 Engine + FFmpeg)
 */

(function () {
  'use strict';

  // ---------------- Default Formats Catalog ----------------
  const ALL_FORMATS = {
    video: [
      { ext: 'mp4', name: 'MP4 Video' },
      { ext: 'mkv', name: 'MKV Matroska' },
      { ext: 'avi', name: 'AVI Video' },
      { ext: 'mov', name: 'MOV QuickTime' },
      { ext: 'webm', name: 'WebM Video' },
      { ext: 'wmv', name: 'WMV Windows Media' },
      { ext: 'flv', name: 'FLV Flash Video' },
      { ext: 'gif', name: 'GIF Animated' },
      { ext: 'mp3', name: 'MP3 Audio (Extract)' },
      { ext: 'wav', name: 'WAV Audio (Extract)' },
      { ext: 'aac', name: 'AAC Audio (Extract)' }
    ],
    photo: [
      { ext: 'png', name: 'PNG Image' },
      { ext: 'jpg', name: 'JPG / JPEG' },
      { ext: 'webp', name: 'WEBP Modern' },
      { ext: 'bmp', name: 'BMP Bitmap' },
      { ext: 'ico', name: 'ICO Windows Icon' },
      { ext: 'tiff', name: 'TIFF Image' },
      { ext: 'gif', name: 'GIF Image' }
    ],
    doc: [
      { ext: 'pdf', name: 'PDF Document' },
      { ext: 'docx', name: 'DOCX Word Document' },
      { ext: 'pptx', name: 'PPTX PowerPoint' },
      { ext: 'txt', name: 'TXT Plain Text' },
      { ext: 'html', name: 'HTML Web Page' },
      { ext: 'md', name: 'MD Markdown' }
    ]
  };

  // ---------------- Application State ----------------
  const state = {
    theme: 'dark',
    railCollapsed: false,
    currentView: 'viewHome',
    appMode: 'convert', // 'convert' or 'compress'
    enabledFormats: {
      video: ['mp4', 'mkv', 'avi', 'mov', 'webm', 'wmv', 'flv', 'gif', 'mp3', 'wav', 'aac'],
      photo: ['png', 'jpg', 'webp', 'bmp', 'ico', 'tiff', 'gif'],
      doc: ['pdf', 'docx', 'pptx', 'txt', 'html', 'md']
    },
    settings: {
      startup: false,
      contextMenu: false,
      hardwareAccel: true,
      soundChime: true,
      outputDestination: 'same'
    },
    selectedFiles: [],
    targetCategory: 'video',
    targetFormat: 'mp4',
    qualityPreset: 'high',
    extractAudio: false,

    // PDF24 Style Compression State
    pdfQuality: 75,
    pdfPpi: 150,
    videoQualityPct: 65,
    videoResolution: 'original',

    lastConvertedFile: null,
    recents: []
  };

  // ---------------- Audio Chime Generator ----------------
  function playCompletionChime() {
    if (!state.settings.soundChime) return;
    try {
      const AudioContext = window.AudioContext || window.webkitAudioContext;
      if (!AudioContext) return;
      const ctx = new AudioContext();
      
      const playTone = (freq, start, duration) => {
        const osc = ctx.createOscillator();
        const gain = ctx.createGain();
        osc.type = 'sine';
        osc.frequency.setValueAtTime(freq, ctx.currentTime + start);
        gain.gain.setValueAtTime(0, ctx.currentTime + start);
        gain.gain.linearRampToValueAtTime(0.18, ctx.currentTime + start + 0.04);
        gain.gain.exponentialRampToValueAtTime(0.001, ctx.currentTime + start + duration);
        osc.connect(gain);
        gain.connect(ctx.destination);
        osc.start(ctx.currentTime + start);
        osc.stop(ctx.currentTime + start + duration);
      };

      playTone(659.25, 0.0, 0.35);
      playTone(830.61, 0.12, 0.45);
      playTone(987.77, 0.24, 0.6);
    } catch (e) {
      console.warn("Audio chime error:", e);
    }
  }

  // ---------------- DOM References ----------------
  const dom = {
    html: document.documentElement,
    navRail: document.getElementById('navRail'),
    railToggleBtn: document.getElementById('railToggleBtn'),
    themeToggleBtn: document.getElementById('themeToggleBtn'),
    themeIconDark: document.getElementById('themeIconDark'),
    themeIconLight: document.getElementById('themeIconLight'),
    navTabs: document.querySelectorAll('.nav-tab'),
    viewPages: document.querySelectorAll('.view-page'),
    
    // Mode Switcher
    modeConvertBtn: document.getElementById('modeConvertBtn'),
    modeCompressBtn: document.getElementById('modeCompressBtn'),
    dropTitle: document.getElementById('dropTitle'),
    dropSubtitle: document.getElementById('dropSubtitle'),
    actionBtnText: document.getElementById('actionBtnText'),

    // Home
    dropZone: document.getElementById('dropZone'),
    fileInput: document.getElementById('fileInput'),
    browseFilesBtn: document.getElementById('browseFilesBtn'),
    browseFolderBtn: document.getElementById('browseFolderBtn'),
    selectionPanel: document.getElementById('selectionPanel'),
    categoryBadge: document.getElementById('categoryBadge'),
    fileCountTitle: document.getElementById('fileCountTitle'),
    clearSelectionBtn: document.getElementById('clearSelectionBtn'),
    fileQueueList: document.getElementById('fileQueueList'),
    
    // Convert section
    convertControlsSection: document.getElementById('convertControlsSection'),
    formatChipsContainer: document.getElementById('formatChipsContainer'),
    qualitySelect: document.getElementById('qualitySelect'),
    extractAudioOpt: document.getElementById('extractAudioOpt'),
    extractAudioCheck: document.getElementById('extractAudioCheck'),
    
    // Compress section (PDF24 style)
    compressControlsSection: document.getElementById('compressControlsSection'),
    pdfCompressBox: document.getElementById('pdfCompressBox'),
    pdfQualitySlider: document.getElementById('pdfQualitySlider'),
    pdfQualityLabel: document.getElementById('pdfQualityLabel'),
    ppiChips: document.querySelectorAll('.ppi-chip'),
    customPpiInput: document.getElementById('customPpiInput'),
    videoCompressBox: document.getElementById('videoCompressBox'),
    videoQualitySlider: document.getElementById('videoQualitySlider'),
    videoQualityLabel: document.getElementById('videoQualityLabel'),
    videoResolutionSelect: document.getElementById('videoResolutionSelect'),

    // Progress Card & Cancel
    progressCard: document.getElementById('progressCard'),
    progressTitle: document.getElementById('progressTitle'),
    progressPercent: document.getElementById('progressPercent'),
    progressBarFill: document.getElementById('progressBarFill'),
    progressDetail: document.getElementById('progressDetail'),
    cancelConvertBtn: document.getElementById('cancelConvertBtn'),
    completionActions: document.getElementById('completionActions'),
    openResultBtn: document.getElementById('openResultBtn'),
    showInFolderBtn: document.getElementById('showInFolderBtn'),
    newConversionBtn: document.getElementById('newConversionBtn'),
    startConvertBtn: document.getElementById('startConvertBtn'),

    // Recents
    recentsList: document.getElementById('recentsList'),
    emptyRecents: document.getElementById('emptyRecents'),
    clearHistoryBtn: document.getElementById('clearHistoryBtn'),

    // Settings
    fmtTabBtns: document.querySelectorAll('.fmt-tab-btn'),
    fmtTabContents: document.querySelectorAll('.fmt-tab-content'),
    videoFormatsGrid: document.getElementById('videoFormatsGrid'),
    photoFormatsGrid: document.getElementById('photoFormatsGrid'),
    docFormatsGrid: document.getElementById('docFormatsGrid'),
    startupSwitch: document.getElementById('startupSwitch'),
    contextMenuSwitch: document.getElementById('contextMenuSwitch'),
    hardwareAccelSwitch: document.getElementById('hardwareAccelSwitch'),
    soundChimeSwitch: document.getElementById('soundChimeSwitch'),
    outputDestinationSelect: document.getElementById('outputDestinationSelect')
  };

  // ---------------- IPC Communication with C++ Backend ----------------
  function sendToCpp(action, payload = {}) {
    const obj = { action, ...payload };
    const rawString = JSON.stringify(obj);
    if (window.chrome && window.chrome.webview) {
      window.chrome.webview.postMessage(rawString);
    } else {
      console.log("[IPC to C++ (Mock)]", obj);
      handleMockCppResponse(action, payload);
    }
  }

  // Listen for messages from C++
  if (window.chrome && window.chrome.webview) {
    window.chrome.webview.addEventListener('message', function (event) {
      let data = event.data;
      if (typeof data === 'string') {
        try { data = JSON.parse(data); } catch (e) { return; }
      }
      handleCppMessage(data);
    });
  }

  function handleCppMessage(data) {
    console.log("[C++ IPC Message]", data);
    switch (data.action) {
      case 'filesSelected':
        if (data.files && data.files.length > 0) {
          addSelectedFiles(data.files);
        }
        break;
      case 'conversionProgress':
        updateProgress(data.percent, data.detail);
        break;
      case 'conversionComplete':
        handleConversionComplete(data.outputFile, data.duration, data.sizeFormatted);
        break;
      case 'conversionCancelled':
        handleConversionCancelled();
        break;
      case 'conversionError':
        handleConversionError(data.error);
        break;
      case 'initialState':
        loadInitialState(data);
        break;
    }
  }

  // ---------------- Mock IPC for Browser Testing ----------------
  function handleMockCppResponse(action, payload) {
    if (action === 'browseFiles') {
      dom.fileInput.click();
    } else if (action === 'startConvert') {
      let pct = 0;
      window._mockInterval = setInterval(() => {
        pct += 20;
        if (pct >= 100) {
          clearInterval(window._mockInterval);
          handleConversionComplete(payload.files[0].path.replace(/\.[^/.]+$/, "_out." + payload.targetFormat), "1.2s", "3.4 MB");
        } else {
          updateProgress(pct, `Processing data block #${pct}...`);
        }
      }, 300);
    } else if (action === 'cancelConvert') {
      clearInterval(window._mockInterval);
      handleConversionCancelled();
    }
  }

  // ---------------- Initialization ----------------
  function init() {
    loadSavedData();
    renderFormatCheckboxes();
    renderRecents();
    setupEventListeners();
    updateThemeUI();
    setAppMode(state.appMode);

    sendToCpp('getInitialState');
  }

  function loadSavedData() {
    try {
      const savedTheme = localStorage.getItem('omni_theme');
      if (savedTheme) state.theme = savedTheme;

      const savedRail = localStorage.getItem('omni_rail_collapsed');
      if (savedRail !== null) state.railCollapsed = (savedRail === 'true');

      const savedFormats = localStorage.getItem('omni_enabled_formats');
      if (savedFormats) state.enabledFormats = JSON.parse(savedFormats);

      const savedSettings = localStorage.getItem('omni_settings');
      if (savedSettings) state.settings = { ...state.settings, ...JSON.parse(savedSettings) };

      const savedRecents = localStorage.getItem('omni_recents');
      if (savedRecents) state.recents = JSON.parse(savedRecents);
    } catch (e) {
      console.warn("Storage load error:", e);
    }

    if (state.railCollapsed) {
      dom.navRail.classList.add('collapsed');
    }
  }

  function saveSettings() {
    try {
      localStorage.setItem('omni_theme', state.theme);
      localStorage.setItem('omni_rail_collapsed', state.railCollapsed);
      localStorage.setItem('omni_enabled_formats', JSON.stringify(state.enabledFormats));
      localStorage.setItem('omni_settings', JSON.stringify(state.settings));
      localStorage.setItem('omni_recents', JSON.stringify(state.recents));
    } catch (e) {
      console.warn("Storage save error:", e);
    }
    sendToCpp('updateSettings', {
      settings: state.settings,
      enabledFormats: state.enabledFormats
    });
  }

  // ---------------- Mode Switcher (Convert vs Compress) ----------------
  function setAppMode(mode) {
    state.appMode = mode;
    dom.modeConvertBtn.classList.toggle('active', mode === 'convert');
    dom.modeCompressBtn.classList.toggle('active', mode === 'compress');

    if (mode === 'convert') {
      dom.dropTitle.textContent = "Drop your files here";
      dom.dropSubtitle.textContent = "Videos, Photos, Documents — all converted locally & offline";
      dom.actionBtnText.textContent = "Convert Now";
      dom.convertControlsSection.classList.remove('hidden');
      dom.compressControlsSection.classList.add('hidden');
    } else {
      dom.dropTitle.textContent = "Drop PDFs or Videos to Compress";
      dom.dropSubtitle.textContent = "Downsample PPI & image quality (PDF24 engine) or shrink video file size";
      dom.actionBtnText.textContent = "Compress Now";
      dom.convertControlsSection.classList.add('hidden');
      dom.compressControlsSection.classList.remove('hidden');
      updateCompressBoxes();
    }
  }

  function updateCompressBoxes() {
    if (state.targetCategory === 'doc' || (state.selectedFiles[0] && state.selectedFiles[0].ext === 'pdf')) {
      dom.pdfCompressBox.classList.remove('hidden');
      dom.videoCompressBox.classList.add('hidden');
    } else {
      dom.pdfCompressBox.classList.add('hidden');
      dom.videoCompressBox.classList.remove('hidden');
    }
  }

  // ---------------- Theme Toggle ----------------
  function toggleTheme() {
    state.theme = (state.theme === 'dark') ? 'light' : 'dark';
    updateThemeUI();
    saveSettings();
  }

  function updateThemeUI() {
    dom.html.setAttribute('data-theme', state.theme);
    if (state.theme === 'dark') {
      dom.themeIconDark.classList.remove('hidden');
      dom.themeIconLight.classList.add('hidden');
    } else {
      dom.themeIconDark.classList.add('hidden');
      dom.themeIconLight.classList.remove('hidden');
    }
  }

  // ---------------- Right Navigation Rail ----------------
  function toggleRail() {
    state.railCollapsed = !state.railCollapsed;
    dom.navRail.classList.toggle('collapsed', state.railCollapsed);
    saveSettings();
  }

  function switchView(viewId) {
    state.currentView = viewId;
    dom.navTabs.forEach(tab => {
      tab.classList.toggle('active', tab.getAttribute('data-view') === viewId);
    });
    dom.viewPages.forEach(page => {
      page.classList.toggle('active', page.id === viewId);
    });
  }

  // ---------------- File Type Detection ----------------
  const EXT_MAP = {
    mp4: 'video', mkv: 'video', avi: 'video', mov: 'video', webm: 'video',
    wmv: 'video', flv: 'video', m4v: 'video', ts: 'video', '3gp': 'video',
    png: 'photo', jpg: 'photo', jpeg: 'photo', webp: 'photo', bmp: 'photo',
    ico: 'photo', tiff: 'photo', tif: 'photo', svg: 'photo',
    pdf: 'doc', docx: 'doc', doc: 'doc', pptx: 'doc', ppt: 'doc',
    txt: 'doc', html: 'doc', htm: 'doc', md: 'doc'
  };

  function detectCategory(ext) {
    ext = (ext || '').toLowerCase().replace(/^\./, '');
    return EXT_MAP[ext] || 'video';
  }

  function formatBytes(bytes) {
    if (!bytes || bytes === 0) return '0 B';
    const k = 1024;
    const sizes = ['B', 'KB', 'MB', 'GB'];
    const i = Math.floor(Math.log(bytes) / Math.log(k));
    return parseFloat((bytes / Math.pow(k, i)).toFixed(1)) + ' ' + sizes[i];
  }

  // ---------------- File Selection Handling ----------------
  function addSelectedFiles(filesList) {
    state.selectedFiles = filesList.map(f => {
      const ext = f.name.split('.').pop().toLowerCase();
      const cat = detectCategory(ext);
      return {
        name: f.name,
        path: f.path || f.name,
        size: f.size || 0,
        ext: ext,
        category: cat
      };
    });

    if (state.selectedFiles.length === 0) return;

    state.targetCategory = state.selectedFiles[0].category;

    const avail = state.enabledFormats[state.targetCategory] || [];
    const currentExt = state.selectedFiles[0].ext;
    let target = avail.find(f => f !== currentExt) || avail[0] || currentExt;
    state.targetFormat = target;

    updateSelectionUI();

    // Clean UI: Hide dropzone, Show selection setup panel, Hide progress
    dom.dropZone.classList.add('hidden');
    dom.selectionPanel.classList.remove('hidden');
    dom.progressCard.classList.add('hidden');
  }

  function updateSelectionUI() {
    dom.categoryBadge.textContent = state.targetCategory.toUpperCase();
    const count = state.selectedFiles.length;
    dom.fileCountTitle.textContent = count === 1 ? state.selectedFiles[0].name : `${count} files selected`;

    dom.fileQueueList.innerHTML = '';
    state.selectedFiles.forEach(f => {
      const item = document.createElement('div');
      item.className = 'queue-item';
      item.innerHTML = `
        <div class="queue-file-info">
          <div class="queue-file-icon">
            <svg viewBox="0 0 24 24" fill="currentColor">
              ${f.category === 'video' ? '<path d="M18 4l2 4h-3l-2-4h-2l2 4h-3l-2-4H8l2 4H7L5 4H4c-1.1 0-1.99.9-1.99 2L2 18c0 1.1.9 2 2 2h16c1.1 0 2-.9 2-2V4h-4z"/>'
                : f.category === 'photo' ? '<path d="M21 19V5c0-1.1-.9-2-2-2H5c-1.1 0-2 .9-2 2v14c0 1.1.9 2 2 2h14c1.1 0 2-.9 2-2zM8.5 13.5l2.5 3.01L14.5 12l4.5 6H5l3.5-4.5z"/>'
                : '<path d="M14 2H6c-1.1 0-1.99.9-1.99 2L4 20c0 1.1.89 2 1.99 2H18c1.1 0 2-.9 2-2V8l-6-6zm2 16H8v-2h8v2zm0-4H8v-2h8v2zm-3-5V3.5L18.5 9H13z"/>'}
            </svg>
          </div>
          <div>
            <div class="queue-file-name" title="${f.path}">${f.name}</div>
            <div class="queue-file-meta">${f.ext.toUpperCase()} • ${formatBytes(f.size)}</div>
          </div>
        </div>
      `;
      dom.fileQueueList.appendChild(item);
    });

    renderFormatChips();
    updateCompressBoxes();

    if (state.targetCategory === 'video') {
      dom.extractAudioOpt.classList.remove('hidden');
      dom.qualitySelect.parentElement.classList.remove('hidden');
    } else if (state.targetCategory === 'photo') {
      dom.extractAudioOpt.classList.add('hidden');
      dom.qualitySelect.parentElement.classList.remove('hidden');
    } else {
      dom.extractAudioOpt.classList.add('hidden');
      dom.qualitySelect.parentElement.classList.add('hidden');
    }
  }

  function renderFormatChips() {
    dom.formatChipsContainer.innerHTML = '';
    const enabledList = state.enabledFormats[state.targetCategory] || [];
    const catalog = ALL_FORMATS[state.targetCategory] || [];

    catalog.forEach(fmt => {
      if (!enabledList.includes(fmt.ext)) return;
      const chip = document.createElement('button');
      chip.className = `format-chip ${state.targetFormat === fmt.ext ? 'active' : ''}`;
      chip.innerHTML = `<span>${fmt.ext.toUpperCase()}</span>`;
      chip.title = fmt.name;
      chip.addEventListener('click', () => {
        state.targetFormat = fmt.ext;
        renderFormatChips();
      });
      dom.formatChipsContainer.appendChild(chip);
    });
  }

  function clearSelection() {
    state.selectedFiles = [];
    dom.selectionPanel.classList.add('hidden');
    dom.progressCard.classList.add('hidden');
    dom.dropZone.classList.remove('hidden');
    dom.fileInput.value = '';
  }

  // ---------------- Conversion & Compression Execution ----------------
  function startOperation() {
    if (state.selectedFiles.length === 0) return;

    const isCompress = (state.appMode === 'compress');
    const targetExt = isCompress ? state.selectedFiles[0].ext : state.targetFormat;

    // Clean UI during conversion
    dom.dropZone.classList.add('hidden');
    dom.selectionPanel.classList.add('hidden');
    dom.progressCard.classList.remove('hidden');
    dom.completionActions.classList.add('hidden');
    dom.cancelConvertBtn.classList.remove('hidden');
    dom.progressBarFill.style.width = '0%';
    dom.progressBarFill.style.background = 'var(--accent-gradient)';
    dom.progressPercent.textContent = '0%';
    dom.progressTitle.textContent = isCompress ? 'Compressing File(s)...' : `Converting to ${targetExt.toUpperCase()}...`;
    dom.progressDetail.textContent = 'Initializing engine and pipelines...';

    sendToCpp('startConvert', {
      files: state.selectedFiles,
      category: state.targetCategory,
      targetFormat: targetExt,
      quality: dom.qualitySelect.value,
      extractAudio: dom.extractAudioCheck.checked,
      destination: dom.outputDestinationSelect.value,

      // Compression Parameters
      isCompressMode: isCompress,
      pdfQuality: parseInt(dom.pdfQualitySlider.value) || 75,
      pdfPpi: parseInt(dom.customPpiInput.value) || 150,
      videoQualityPct: parseInt(dom.videoQualitySlider.value) || 65,
      videoResolution: dom.videoResolutionSelect.value
    });
  }

  function cancelOperation() {
    dom.progressDetail.textContent = 'Cancelling operation...';
    sendToCpp('cancelConvert');
  }

  function handleConversionCancelled() {
    dom.progressTitle.textContent = 'Operation Cancelled';
    dom.progressDetail.textContent = 'Process was cancelled by the user.';
    dom.progressBarFill.style.background = 'var(--danger)';
    dom.cancelConvertBtn.classList.add('hidden');
    dom.completionActions.classList.remove('hidden');
  }

  function updateProgress(percent, detail) {
    dom.progressBarFill.style.width = `${percent}%`;
    dom.progressPercent.textContent = `${percent}%`;
    if (detail) dom.progressDetail.textContent = detail;
  }

  function handleConversionComplete(outputFile, duration, size) {
    dom.progressBarFill.style.width = '100%';
    dom.progressPercent.textContent = '100%';
    dom.progressTitle.textContent = state.appMode === 'compress' ? 'Compression Complete!' : 'Conversion Successful!';
    dom.progressDetail.textContent = `Output saved: ${outputFile}`;
    dom.cancelConvertBtn.classList.add('hidden');
    dom.completionActions.classList.remove('hidden');
    dom.openResultBtn.classList.remove('hidden');
    dom.showInFolderBtn.classList.remove('hidden');

    state.lastConvertedFile = outputFile;
    playCompletionChime();

    if (state.selectedFiles.length > 0) {
      const source = state.selectedFiles[0];
      const isCompress = (state.appMode === 'compress');
      const recItem = {
        id: Date.now(),
        name: outputFile.split(/[/\\]/).pop(),
        path: outputFile,
        fromExt: source.ext.toUpperCase(),
        toExt: isCompress ? `${source.ext.toUpperCase()} (Comp)` : state.targetFormat.toUpperCase(),
        category: state.targetCategory,
        timestamp: new Date().toLocaleTimeString([], { hour: '2-digit', minute: '2-digit' }),
        date: new Date().toLocaleDateString(),
        size: size || formatBytes(source.size)
      };
      state.recents.unshift(recItem);
      if (state.recents.length > 40) state.recents.pop();
      saveSettings();
      renderRecents();
    }
  }

  function handleConversionError(errorMsg) {
    dom.progressTitle.textContent = 'Operation Failed';
    dom.progressDetail.textContent = `${errorMsg}`;
    dom.progressBarFill.style.background = 'var(--danger)';
    dom.cancelConvertBtn.classList.add('hidden');
    dom.completionActions.classList.remove('hidden');
    dom.openResultBtn.classList.add('hidden');
    dom.showInFolderBtn.classList.add('hidden');
    state.lastConvertedFile = null;
  }

  // ---------------- Recents Page ----------------
  function renderRecents() {
    dom.recentsList.innerHTML = '';
    if (state.recents.length === 0) {
      dom.emptyRecents.classList.remove('hidden');
      return;
    }

    dom.emptyRecents.classList.add('hidden');
    state.recents.forEach(item => {
      const card = document.createElement('div');
      card.className = 'recent-item-card';
      card.innerHTML = `
        <div class="recent-left">
          <div class="recent-icon-wrap">
            <svg viewBox="0 0 24 24" fill="currentColor">
              ${item.category === 'video' ? '<path d="M18 4l2 4h-3l-2-4h-2l2 4h-3l-2-4H8l2 4H7L5 4H4c-1.1 0-1.99.9-1.99 2L2 18c0 1.1.9 2 2 2h16c1.1 0 2-.9 2-2V4h-4z"/>'
                : item.category === 'photo' ? '<path d="M21 19V5c0-1.1-.9-2-2-2H5c-1.1 0-2 .9-2 2v14c0 1.1.9 2 2 2h14c1.1 0 2-.9 2-2zM8.5 13.5l2.5 3.01L14.5 12l4.5 6H5l3.5-4.5z"/>'
                : '<path d="M14 2H6c-1.1 0-1.99.9-1.99 2L4 20c0 1.1.89 2 1.99 2H18c1.1 0 2-.9 2-2V8l-6-6zm2 16H8v-2h8v2zm0-4H8v-2h8v2zm-3-5V3.5L18.5 9H13z"/>'}
            </svg>
          </div>
          <div class="recent-details">
            <span class="recent-name" title="${item.path}">${item.name}</span>
            <div class="recent-meta-row">
              <span class="format-flow-pill">${item.fromExt} ➔ ${item.toExt}</span>
              <span>•</span>
              <span>${item.date} ${item.timestamp}</span>
              <span>•</span>
              <span>${item.size}</span>
            </div>
          </div>
        </div>
        <div class="recent-actions">
          <button class="md-btn secondary-btn" data-action="open" data-path="${encodeURIComponent(item.path)}">
            <svg viewBox="0 0 24 24" class="btn-icon-small" fill="currentColor"><path d="M19 19H5V5h7V3H5c-1.11 0-2 .9-2 2v14c0 1.1.89 2 2 2h14c1.1 0 2-.9 2-2v-7h-2v7zM14 3v2h3.59l-9.83 9.83 1.41 1.41L19 6.41V10h2V3h-7z"/></svg>
            Open
          </button>
          <button class="md-btn ghost-btn" data-action="folder" data-path="${encodeURIComponent(item.path)}">
            <svg viewBox="0 0 24 24" class="btn-icon-small" fill="currentColor"><path d="M20 6h-8l-2-2H4c-1.1 0-1.99.9-1.99 2L2 18c0 1.1.9 2 2 2h16c1.1 0 2-.9 2-2V8c0-1.1-.9-2-2-2zm0 12H4V8h16v10z"/></svg>
            Folder
          </button>
        </div>
      `;

      card.querySelectorAll('button[data-action]').forEach(btn => {
        btn.addEventListener('click', () => {
          const act = btn.getAttribute('data-action');
          const path = decodeURIComponent(btn.getAttribute('data-path'));
          if (act === 'open') {
            sendToCpp('openFile', { path });
          } else if (act === 'folder') {
            sendToCpp('showInExplorer', { path });
          }
        });
      });

      dom.recentsList.appendChild(card);
    });
  }

  function clearHistory() {
    state.recents = [];
    saveSettings();
    renderRecents();
    sendToCpp('clearRecents');
  }

  // ---------------- Settings Page Format Availability ----------------
  function renderFormatCheckboxes() {
    ['video', 'photo', 'doc'].forEach(cat => {
      const container = cat === 'video' ? dom.videoFormatsGrid
        : cat === 'photo' ? dom.photoFormatsGrid
        : dom.docFormatsGrid;
      container.innerHTML = '';

      ALL_FORMATS[cat].forEach(fmt => {
        const isChecked = state.enabledFormats[cat].includes(fmt.ext);
        const label = document.createElement('label');
        label.className = 'fmt-checkbox-card';
        label.innerHTML = `
          <input type="checkbox" data-cat="${cat}" data-fmt="${fmt.ext}" ${isChecked ? 'checked' : ''}>
          <span>${fmt.ext.toUpperCase()}</span>
        `;

        label.querySelector('input').addEventListener('change', (e) => {
          const checked = e.target.checked;
          const list = state.enabledFormats[cat];
          if (checked && !list.includes(fmt.ext)) {
            list.push(fmt.ext);
          } else if (!checked && list.includes(fmt.ext)) {
            const idx = list.indexOf(fmt.ext);
            if (idx > -1) list.splice(idx, 1);
          }
          saveSettings();
          if (state.selectedFiles.length > 0) renderFormatChips();
        });

        container.appendChild(label);
      });
    });

    dom.fmtTabBtns.forEach(btn => {
      btn.addEventListener('click', () => {
        const targetId = btn.getAttribute('data-fmt-tab');
        dom.fmtTabBtns.forEach(b => b.classList.toggle('active', b === btn));
        dom.fmtTabContents.forEach(c => c.classList.toggle('active', c.id === targetId));
      });
    });

    dom.startupSwitch.checked = state.settings.startup;
    dom.startupSwitch.addEventListener('change', (e) => {
      state.settings.startup = e.target.checked;
      saveSettings();
      sendToCpp('toggleStartup', { enabled: state.settings.startup });
    });

    dom.contextMenuSwitch.checked = state.settings.contextMenu;
    dom.contextMenuSwitch.addEventListener('change', (e) => {
      state.settings.contextMenu = e.target.checked;
      saveSettings();
      sendToCpp('toggleContextMenu', { enabled: state.settings.contextMenu });
    });

    dom.hardwareAccelSwitch.checked = state.settings.hardwareAccel;
    dom.hardwareAccelSwitch.addEventListener('change', (e) => {
      state.settings.hardwareAccel = e.target.checked;
      saveSettings();
    });

    dom.soundChimeSwitch.checked = state.settings.soundChime;
    dom.soundChimeSwitch.addEventListener('change', (e) => {
      state.settings.soundChime = e.target.checked;
      saveSettings();
    });

    dom.outputDestinationSelect.value = state.settings.outputDestination;
    dom.outputDestinationSelect.addEventListener('change', (e) => {
      state.settings.outputDestination = e.target.value;
      saveSettings();
    });
  }

  // ---------------- Event Listeners ----------------
  function setupEventListeners() {
    dom.themeToggleBtn.addEventListener('click', toggleTheme);
    dom.railToggleBtn.addEventListener('click', toggleRail);

    dom.modeConvertBtn.addEventListener('click', () => setAppMode('convert'));
    dom.modeCompressBtn.addEventListener('click', () => setAppMode('compress'));

    dom.pdfQualitySlider.addEventListener('input', (e) => {
      dom.pdfQualityLabel.textContent = `${e.target.value}%`;
    });

    dom.videoQualitySlider.addEventListener('input', (e) => {
      const v = parseInt(e.target.value);
      dom.videoQualityLabel.textContent = v < 40 ? `${v}% (Ultra Compact)` : v < 70 ? `${v}% (Balanced)` : `${v}% (High Quality)`;
    });

    dom.ppiChips.forEach(chip => {
      chip.addEventListener('click', () => {
        dom.ppiChips.forEach(c => c.classList.toggle('active', c === chip));
        const ppi = chip.getAttribute('data-ppi');
        dom.customPpiInput.value = ppi;
      });
    });

    dom.customPpiInput.addEventListener('input', (e) => {
      const val = e.target.value;
      dom.ppiChips.forEach(c => c.classList.toggle('active', c.getAttribute('data-ppi') === val));
    });

    dom.navTabs.forEach(tab => {
      tab.addEventListener('click', () => {
        const view = tab.getAttribute('data-view');
        switchView(view);
      });
    });

    // Dropzone interactions
    dom.dropZone.addEventListener('dragover', (e) => {
      e.preventDefault();
      dom.dropZone.classList.add('dragover');
    });

    dom.dropZone.addEventListener('dragleave', () => {
      dom.dropZone.classList.remove('dragover');
    });

    dom.dropZone.addEventListener('drop', (e) => {
      e.preventDefault();
      dom.dropZone.classList.remove('dragover');
      if (e.dataTransfer.files && e.dataTransfer.files.length > 0) {
        handleDroppedFiles(e.dataTransfer.files);
      }
    });

    dom.dropZone.addEventListener('click', (e) => {
      if (e.target.closest('#browseFilesBtn') || e.target.closest('#browseFolderBtn')) return;
      sendToCpp('browseFiles');
    });

    dom.browseFilesBtn.addEventListener('click', (e) => {
      e.stopPropagation();
      sendToCpp('browseFiles');
    });

    dom.browseFolderBtn.addEventListener('click', (e) => {
      e.stopPropagation();
      sendToCpp('browseFolder');
    });

    dom.fileInput.addEventListener('change', (e) => {
      if (e.target.files && e.target.files.length > 0) {
        handleDroppedFiles(e.target.files);
      }
    });

    dom.clearSelectionBtn.addEventListener('click', clearSelection);
    dom.startConvertBtn.addEventListener('click', startOperation);
    dom.cancelConvertBtn.addEventListener('click', cancelOperation);

    dom.openResultBtn.addEventListener('click', () => {
      if (state.lastConvertedFile) {
        sendToCpp('openFile', { path: state.lastConvertedFile });
      }
    });

    dom.showInFolderBtn.addEventListener('click', () => {
      if (state.lastConvertedFile) {
        sendToCpp('showInExplorer', { path: state.lastConvertedFile });
      }
    });

    dom.newConversionBtn.addEventListener('click', () => {
      clearSelection();
    });

    dom.clearHistoryBtn.addEventListener('click', clearHistory);
  }

  function handleDroppedFiles(fileList) {
    const arr = [];
    for (let i = 0; i < fileList.length; i++) {
      const f = fileList[i];
      arr.push({
        name: f.name,
        path: f.path || f.name,
        size: f.size || 0
      });
    }
    if (arr.length > 0) {
      addSelectedFiles(arr);
    }
  }

  function loadInitialState(backendState) {
    if (backendState.settings) {
      state.settings = { ...state.settings, ...backendState.settings };
      dom.startupSwitch.checked = state.settings.startup;
      dom.contextMenuSwitch.checked = state.settings.contextMenu;
    }
    if (backendState.startupFiles && backendState.startupFiles.length > 0) {
      addSelectedFiles(backendState.startupFiles);
    }
  }

  document.addEventListener('DOMContentLoaded', init);
})();
