'use strict';

const enableDebug = document.location.hostname === 'localhost';

function addDelay(func, minDelay, maxDelay) {
  return async function(...args) {
    const delay = minDelay + Math.random() * (maxDelay - minDelay);
    await new Promise(r => setTimeout(r, delay));
    return await func(...args);
  }
}

if (enableDebug) fetch = addDelay(fetch, 250, 750);

async function fetchPageByRef(ref) {
  const response = await fetch(`api/page?page=${encodeURIComponent(ref)}`);
  return await response.json();
}

class PageInput {
  constructor(titleElem, randomizeButton, enabled, onEnabledChanged) {
    this.titleElem = titleElem;
    this.randomizeButton = randomizeButton;
    this.enabled = enabled;
    this.onEnabledChanged = onEnabledChanged;
    this.error = null;
    randomizeButton.onclick = this.randomize.bind(this);
  }

  setEnabled(enabled) {
    if (this.enabled == enabled) return false;
    this.enabled = enabled;
    this.titleElem.disabled = !enabled;
    this.randomizeButton.disabled = !enabled;
    this.onEnabledChanged(enabled);
    return true;
  }

  setError(message) {
    this.error = message;
    this.titleElem.classList.toggle('error', message);
  }

  getTitle() {
    return this.titleElem.value;
  }

  setTitle(title) {
    this.titleElem.value = title;
  }

  async randomize() {
    this.setEnabled(false);
    try {
      const json = await fetchPageByRef('?');
      const title = json?.page?.title;
      if (title) this.setTitle(title);
    } finally {
      this.setEnabled(true);
    }
  }
}

class SearchButtons {
  constructor(searchButton, surpriseMeButton, enabled) {
    this.searchButton = searchButton;
    this.surpriseMeButton = surpriseMeButton;
    this.enabled = enabled;
  }

  setEnabled(enabled) {
    if (this.enabled == enabled) return false;
    this.enabled = enabled;
    this.searchButton.disabled = !enabled;
    this.surpriseMeButton.disabled = !enabled;
    return true;
  }
}

class App {
  constructor() {
    this.wikipediaBaseUrl = 'https://en.wikipedia.org/wiki/';
    this.startPageInput = new PageInput(
      document.getElementById('start-page-title-text'),
      document.getElementById('start-page-randomize-button'),
      /* enabled= */ false,
      this.updateSearchEnabled.bind(this),
    );
    this.finishPageInput = new PageInput(
      document.getElementById('finish-page-title-text'),
      document.getElementById('finish-page-randomize-button'),
      /* enabled= */ false,
      this.updateSearchEnabled.bind(this),
    );
    const searchButton = document.getElementById('search-button');
    const surpriseMeButton = document.getElementById('surprise-me-button');
    this.searchButtons = new SearchButtons(searchButton, surpriseMeButton);
    this.searchInProgress = false;
    searchButton.onclick = this.handleSearchClick.bind(this);
    surpriseMeButton.onclick = this.handleSurpriseMeClick.bind(this);
    this.loadCorpusInfo();
    this.startPageInput.randomize();
    this.finishPageInput.randomize();
  }

  updateSearchEnabled() {
    this.searchButtons.setEnabled(
      !this.searchInProgress &&
      this.startPageInput.enabled &&
      this.finishPageInput.enabled);
  }

  async loadCorpusInfo() {
    const response = await fetch('api/corpus');
    const json = await response.json();
    const graph_filename = json?.corpus?.graph_filename;
    const vertex_count = json?.corpus?.vertex_count;
    const edge_count = json?.corpus?.edge_count;
    if (typeof graph_filename === 'string' &&
        typeof vertex_count === 'number' &&
        typeof edge_count === 'number') {
      const formatNumber = Intl.NumberFormat('en-US').format;
      document.getElementById('corpus-graph').textContent = graph_filename;
      document.getElementById('corpus-pages').textContent = formatNumber(vertex_count - 1);
      document.getElementById('corpus-links').textContent = formatNumber(edge_count);
      document.getElementById('corpus-info').style.display = null;
      const base_url = json?.corpus?.base_url;
      if (typeof(base_url) === 'string') this.wikipediaBaseUrl = base_url
    } else {
      console.warn('Invalid corpus info:', json);
    }
  }

  async handleSearchClick() {
    const start = this.startPageInput.getTitle();
    const finish = this.finishPageInput.getTitle();
    return await this.search(start, finish);
  }

  async handleSurpriseMeClick() {
    return await this.search('?', '?');
  }

  async search(start, finish) {
    this.searchInProgress = true;
    this.updateSearchEnabled();
    try {
      const response = await fetch(`api/shortest-path?start=${encodeURIComponent(start)}&finish=${encodeURIComponent(finish)}`);
      const json = await response.json();

      const startTitle = json?.start?.page?.title;
      if (startTitle) {
        this.startPageInput.setTitle(startTitle);
      } else {
        // TODO: show error
      }

      const finishTitle = json?.finish?.page?.title;
      if (finishTitle) {
        this.finishPageInput.setTitle(finishTitle);
      } else {
        // TODO: show error
      }

      const pageFoundElem = document.getElementById('path-found');
      const pageNotfoundElem = document.getElementById('path-not-found');
      const searchErrorElem = document.getElementById('search-error');
      pageFoundElem.style.display = 'none';
      pageNotfoundElem.style.display = 'none';

      const path = json?.path;
      if (Array.isArray(path)) {
        if (path.length > 0) {
          pageFoundElem.style.display = null;
          const ol = document.getElementById('path-list');
          ol.replaceChildren();
          for (const elem of path) {
            const title = elem?.page?.title;
            const a = document.createElement('a');
            a.href = this.wikipediaBaseUrl + encodeURI(title);
            a.textContent = title;
            a.target = '_blank';
            const li = document.createElement('li');
            li.appendChild(a);
            const displayedAs = elem?.displayed_as;
            if (displayedAs) {
              const em = document.createElement('em');
              em.appendChild(document.createTextNode(displayedAs));
              li.appendChild(document.createTextNode(' (displayed as: '));
              li.appendChild(em);
              li.appendChild(document.createTextNode(')'));
            }
            ol.appendChild(li);
          }
        } else {
          pageNotfoundElem.style.display = null;
          // TODO: show details
          console.warn('path not found');
        }
      } else {
        document.getElementById('search-error-message').textContent = json?.error?.message ?? 'Missing error!';
        searchErrorElem.style.display = null;
      }
    } finally {
      this.searchInProgress = false;
      this.updateSearchEnabled();
    }
  }
}

// TODO: handle errors from RPC endpoints!
