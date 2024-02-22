'use strict';

// Add '#debug=1' to the URL to enable debug features, like delayed loading.
const hashParams = new URLSearchParams(document.location.hash.substring(1));
const enableDebug = hashParams.get('debug') > 0;

// Keep this in sync with --path-animation-duration in style.css
const pathAnimationDuration = 0.5;

const formatNumber = Intl.NumberFormat('en-US').format;

function addDelay(func, minDelay, maxDelay) {
  return async function(...args) {
    const delay = minDelay + Math.random() * (maxDelay - minDelay);
    await new Promise(r => setTimeout(r, delay));
    return await func(...args);
  }
}

if (enableDebug) fetch = addDelay(fetch, 250, 750);

async function fetchJson(url) {
  const response = await fetch(url);
  if (response.headers.get('Content-Type') === 'application/json') {
    return await response.json();
  }
  throw new Error(`HTTP ${response.status}: ${await response.text()}`);
}

async function fetchPageByRef(ref) {
  return await fetchJson(`api/page?page=${encodeURIComponent(ref)}`)
}

class PageInput {
  constructor(titleElem, randomizeButton, enabled, onEnabledChanged) {
    this.titleElem = titleElem;
    this.randomizeButton = randomizeButton;
    this.enabled = enabled;
    this.onEnabledChanged = onEnabledChanged;
    this.error = null;
    randomizeButton.onclick = this.randomize.bind(this);
    titleElem.onfocus = titleElem.onchange = this.clearError.bind(this);
  }

  setEnabled(enabled) {
    if (this.enabled == enabled) return false;
    this.enabled = enabled;
    this.titleElem.disabled = !enabled;
    this.randomizeButton.disabled = !enabled;
    this.onEnabledChanged(enabled);
    return true;
  }

  clearError() {
    this.setError(null);
  }

  setError(message) {
    this.error = message;
    this.titleElem.classList.toggle('error', Boolean(message));
  }

  getTitle() {
    return this.titleElem.value;
  }

  setTitle(title) {
    if (this.titleElem.value != title) {
      this.titleElem.value = title;
      this.clearError();
    }
  }

  async randomize() {
    this.setEnabled(false);
    try {
      const json = await fetchPageByRef('?');
      const title = json?.page?.title;
      if (title) this.setTitle(title);
    } catch (e) {
      console.error('Failed to fetch page info', e);
      this.showError('Failed to fetch page info', e.message);
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
    document.getElementById('error-close').onclick = this.hideError.bind(this);
  }

  updateSearchEnabled() {
    this.searchButtons.setEnabled(
      !this.searchInProgress &&
      this.startPageInput.enabled &&
      this.finishPageInput.enabled);
  }

  hideError() {
    document.getElementById('error').style.display = 'none';
  }

  showError(title, message) {
    const errorDiv = document.getElementById('error');
    document.getElementById('error-title').textContent = title || '';
    document.getElementById('error-message').textContent = message || '';
    errorDiv.style.display = null;
  }

  async loadCorpusInfo() {
    try {
      const json = await fetchJson('api/corpus');
      const graph_filename = json?.corpus?.graph_filename;
      const vertex_count = json?.corpus?.vertex_count;
      const edge_count = json?.corpus?.edge_count;
      document.getElementById('corpus-graph').textContent =
          typeof graph_filename === 'string' ? graph_filename : '';
      document.getElementById('corpus-pages').textContent =
          typeof vertex_count === 'number' ? formatNumber(vertex_count - 1) : '';
      document.getElementById('corpus-links').textContent =
          typeof edge_count === 'number' ? formatNumber(edge_count) : '';
      document.getElementById('corpus-info').style.display = null;
      const base_url = json?.corpus?.base_url;
      if (typeof(base_url) === 'string') this.wikipediaBaseUrl = base_url;
    } catch (e) {
      console.error('Failed to fetch corpus info', e);
      this.showError('Failed to fetch corpus info', e.message);
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
      const pathFoundElem = document.getElementById('path-found');
      const pageNotfoundElem = document.getElementById('path-not-found');
      const searchStatsElem = document.getElementById('search-stats');
      pathFoundElem.style.display = 'none';
      pageNotfoundElem.style.display = 'none';
      searchStatsElem.style.display = 'none';
      this.hideError();

      const json = await fetchJson(`api/shortest-path?start=${encodeURIComponent(start)}&finish=${encodeURIComponent(finish)}`);

      const startTitle = json?.start?.page?.title;
      const startErrorMessage = json?.start?.error?.message;
      if (startTitle) {
        this.startPageInput.setTitle(startTitle);
      } else if (typeof startErrorMessage === 'string') {
        this.startPageInput.setError(startErrorMessage);
      }

      const finishTitle = json?.finish?.page?.title;
      const finishErrorMessage = json?.finish?.error?.message;
      if (finishTitle) {
        this.finishPageInput.setTitle(finishTitle);
      } else {
        this.finishPageInput.setError(finishErrorMessage);
      }

      if (json?.error) {
        this.showError('Search failed', json?.error?.message);
        return;
      }

      const makePageSpan = (json) => {
        const pageSpan = document.createElement('span');
        pageSpan.className = 'page';
        const title = json?.page?.title;
        const a = document.createElement('a');
        a.href = this.wikipediaBaseUrl + encodeURI(title);
        a.textContent = title;
        a.target = '_blank';
        pageSpan.appendChild(a);
        const displayedAs = json?.displayed_as;
        if (displayedAs) {
          pageSpan.appendChild(document.createElement('br'));
          const em = document.createElement('em');
          em.appendChild(document.createTextNode(displayedAs));
          pageSpan.appendChild(document.createTextNode(' (displayed as: '));
          pageSpan.appendChild(em);
          pageSpan.appendChild(document.createTextNode(')'));
        }
        return pageSpan;
      };

      const replaceElemById = (oldElemId, newElem) => {
        const elem = document.getElementById(oldElemId);
        if (!elem) {
          console.warn(`Element with id "${oldElemId}" not found!`);
          return;
        }
        newElem.id = oldElemId;
        elem.parentElement.replaceChild(newElem, elem);
      }

      const path = json?.path;
      const animate = document.getElementById('animation-checkbox').checked;
      if (Array.isArray(path)) {
        if (path.length > 0) {
          pathFoundElem.style.display = null;
          const pathListElem = document.getElementById('path-found-list');
          pathListElem.replaceChildren();
          for (var i = 0; i < path.length; ++i) {
            if (i > 0) {
              const linkDiv = document.createElement('div');
              linkDiv.className = 'link';
              if (animate) {
                linkDiv.classList.add('animated');
                linkDiv.style.animationDelay = `${pathAnimationDuration * (i - 1)}s`;
              }
              pathListElem.appendChild(linkDiv);
            }
            const rowDiv = document.createElement('div');
            rowDiv.className = 'row';
            const indexSpan = document.createElement('span');
            indexSpan.className = 'index';
            indexSpan.appendChild(document.createTextNode(String(i + 1)));
            rowDiv.appendChild(indexSpan);
            const pageSpan = makePageSpan(path[i]);
            rowDiv.appendChild(pageSpan);
            if (animate && i > 0) {
              indexSpan.classList.add('animated');
              indexSpan.style.animationDelay = `${pathAnimationDuration * (i - 1)}s`;
              indexSpan.style.zIndex = path.length - i;
              pageSpan.classList.add('animated');
              pageSpan.style.animationDelay = `${pathAnimationDuration * (i - 1)}s`;
            }
            pathListElem.appendChild(rowDiv);
          }
        } else {
          pageNotfoundElem.style.display = null;
          replaceElemById('path-not-found-start', makePageSpan(json?.start));
          replaceElemById('path-not-found-finish', makePageSpan(json?.finish));
        }
        if (json?.stats) {
          searchStatsElem.style.display = null;
          const stats = json.stats;
          document.getElementById('vertices-reached').textContent = formatNumber(stats.vertices_reached);
          document.getElementById('vertices-expanded').textContent = formatNumber(stats.vertices_expanded);
          document.getElementById('edges-expanded').textContent = formatNumber(stats.edges_expanded);
          document.getElementById('time-taken-ms').textContent = formatNumber(stats.time_taken_ms);
        }
      } else {
        document.getElementById('search-error-message').textContent = json?.error?.message ?? 'Missing error!';
        searchErrorElem.style.display = null;
      }
    } catch (e) {
      console.error('Search failed', e);
      this.showError('Search failed', e.message);
    } finally {
      this.searchInProgress = false;
      this.updateSearchEnabled();
    }
  }
}

// TODO: handle errors from RPC endpoints!
