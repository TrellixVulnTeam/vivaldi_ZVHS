// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://webui-test/mojo_webui_test_support.js';
import 'chrome://new-tab-page/new_tab_page.js';

import {$$, decodeString16, mojoString16, RealboxBrowserProxy, RealboxElement, RealboxIconElement, RealboxMatchElement} from 'chrome://new-tab-page/new_tab_page.js';
import {AutocompleteMatch} from 'chrome://new-tab-page/realbox.mojom-webui.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {getDeepActiveElement} from 'chrome://resources/js/util.m.js';
import {assertEquals, assertFalse, assertNotEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

import {assertStyle, createTheme} from '../test_support.js';

import {TestRealboxBrowserProxy} from './test_realbox_browser_proxy.js';

enum Classes {
  SELECTED = 'selected',
}

function createClipboardEvent(name: string): ClipboardEvent {
  return new ClipboardEvent(
      name, {cancelable: true, clipboardData: new DataTransfer()});
}

function createAutocompleteMatch(): AutocompleteMatch {
  return {
    a11yLabel: mojoString16(''),
    allowedToBeDefaultMatch: false,
    isSearchType: false,
    swapContentsAndDescription: false,
    supportsDeletion: false,
    suggestionGroupId: -1,  // Indicates a missing suggestion group Id.
    contents: mojoString16(''),
    contentsClass: [{offset: 0, style: 0}],
    description: mojoString16(''),
    descriptionClass: [{offset: 0, style: 0}],
    destinationUrl: {url: ''},
    inlineAutocompletion: mojoString16(''),
    fillIntoEdit: mojoString16(''),
    iconUrl: '',
    imageDominantColor: '',
    imageUrl: '',
    removeButtonA11yLabel: mojoString16(''),
    type: '',
    action: undefined,
    answer: undefined,
    isRichSuggestion: false,
    tailSuggestCommonPrefix: undefined,
  };
}

function createUrlMatch(modifiers: Partial<AutocompleteMatch> = {}):
    AutocompleteMatch {
  return Object.assign(
      createAutocompleteMatch(), {
        swapContentsAndDescription: true,
        contents: mojoString16('helloworld.com'),
        contentsClass: [{offset: 0, style: 1}],
        destinationUrl: {url: 'https://helloworld.com/'},
        fillIntoEdit: mojoString16('https://helloworld.com'),
        type: 'url-what-you-typed',
      },
      modifiers);
}

function createSearchMatch(modifiers: Partial<AutocompleteMatch> = {}):
    AutocompleteMatch {
  return Object.assign(
      createAutocompleteMatch(), {
        isSearchType: true,
        contents: mojoString16('hello world'),
        contentsClass: [{offset: 0, style: 0}],
        description: mojoString16('Google search'),
        descriptionClass: [{offset: 0, style: 4}],
        destinationUrl: {url: 'https://www.google.com/search?q=hello+world'},
        fillIntoEdit: mojoString16('hello world'),
        type: 'search-what-you-typed',
      },
      modifiers);
}

function createCalculatorMatch(modifiers: Partial<AutocompleteMatch>):
    AutocompleteMatch {
  return Object.assign(
      createAutocompleteMatch(), {
        isSearchType: true,
        contents: mojoString16('2 + 3'),
        contentsClass: [{offset: 0, style: 0}],
        description: mojoString16('5'),
        descriptionClass: [{offset: 0, style: 0}],
        destinationUrl: {url: 'https://www.google.com/search?q=2+%2B+3'},
        fillIntoEdit: mojoString16('5'),
        type: 'search-calculator-answer',
        iconUrl: 'calculator.svg',
      },
      modifiers);
}

/** Verifies the autocomplete match is showing. */
function verifyMatch(match: AutocompleteMatch, matchEl: RealboxMatchElement) {
  assertEquals('option', matchEl.getAttribute('role'));
  const matchContents =
      decodeString16(match.answer ? match.answer.firstLine : match.contents);
  const matchDescription = decodeString16(
      match.answer ? match.answer.secondLine : match.description);
  const separatorText =
      matchDescription ? loadTimeData.getString('realboxSeparator') : '';
  const contents = matchEl.$['contents'].textContent!.trim();
  const separator = matchEl.$['separator'].textContent!.trim();
  const description = matchEl.$['description'].textContent!.trim();
  const text = (contents + ' ' + separator + ' ' + description).trim();
  assertEquals(
      match.swapContentsAndDescription ?
          matchDescription + separatorText + matchContents :
          matchContents + separatorText + matchDescription,
      text);
}

suite('NewTabPageRealboxTest', () => {
  let realbox: RealboxElement;

  let testProxy: TestRealboxBrowserProxy;

  suiteSetup(() => {
    loadTimeData.overrideValues({
      realboxMatchOmniboxTheme: true,
      realboxSeparator: ' - ',
    });
  });

  setup(async () => {
    document.body.innerHTML = '';

    testProxy = new TestRealboxBrowserProxy();
    RealboxBrowserProxy.setInstance(testProxy);

    realbox = document.createElement('ntp-realbox');
    document.body.appendChild(realbox);
  });

  function assertIconBackgroundImageUrl(
      iconElement: RealboxIconElement, url: string) {
    assertStyle(
        iconElement.$.icon, 'background-image',
        `url("chrome://new-tab-page/${url}")`);
    assertStyle(iconElement.$.icon, '-webkit-mask-image', 'none');
  }

  function assertIconMaskImageUrl(
      iconElement: RealboxIconElement, url: string) {
    assertStyle(
        iconElement.$.icon, '-webkit-mask-image',
        `url("chrome://new-tab-page/${url}")`);
    assertStyle(iconElement.$.icon, 'background-image', 'none');
  }

  function areMatchesShowing(): boolean {
    return window.getComputedStyle(realbox.$.matches).display !== 'none';
  }

  test('when created is not focused and matches are not showing', () => {
    assertFalse(realbox.hidden);
    assertNotEquals(realbox, getDeepActiveElement());
    assertFalse(areMatchesShowing());
  });

  test('clicking voice search button send voice search event', async () => {
    // Arrange.
    const whenOpenVoiceSearch = eventToPromise('open-voice-search', realbox);

    // Act.
    realbox.$.voiceSearchButton.click();

    // Assert.
    await whenOpenVoiceSearch;
  });

  test('setting theme updates realbox', async () => {
    const matches = realbox.$.matches;
    // Assert.
    assertStyle(realbox, '--search-box-bg', '');
    assertStyle(realbox, '--search-box-placeholder', '');
    assertStyle(realbox, '--search-box-results-bg', '');
    assertStyle(realbox, '--search-box-text', '');
    assertStyle(realbox, '--search-box-icon', '');
    assertStyle(matches, '--search-box-icon', '');
    assertStyle(matches, '--search-box-results-bg-hovered', '');
    assertStyle(matches, '--search-box-results-bg-selected', '');
    assertStyle(matches, '--search-box-results-bg', '');
    assertStyle(matches, '--search-box-results-dim-selected', '');
    assertStyle(matches, '--search-box-results-dim', '');
    assertStyle(matches, '--search-box-results-text-selected', '');
    assertStyle(matches, '--search-box-results-text', '');
    assertStyle(matches, '--search-box-results-url-selected', '');
    assertStyle(matches, '--search-box-results-url', '');

    // Act.
    realbox.theme = createTheme().searchBox;

    // Assert.
    assertStyle(realbox, '--search-box-bg', 'rgba(0, 0, 0, 1)');
    assertStyle(realbox, '--search-box-placeholder', 'rgba(0, 0, 3, 1)');
    assertStyle(realbox, '--search-box-results-bg', 'rgba(0, 0, 4, 1)');
    assertStyle(realbox, '--search-box-text', 'rgba(0, 0, 13, 1)');
    assertStyle(matches, '--search-box-icon-selected', 'rgba(0, 0, 2, 1)');
    assertStyle(matches, '--search-box-results-bg-hovered', 'rgba(0, 0, 5, 1)');
    assertStyle(matches, '--search-box-results-bg', 'rgba(0, 0, 4, 1)');
    assertStyle(
        matches, '--search-box-results-dim-selected', 'rgba(0, 0, 8, 1)');
    assertStyle(matches, '--search-box-results-dim', 'rgba(0, 0, 7, 1)');
    assertStyle(matches, '--search-box-results-text', 'rgba(0, 0, 9, 1)');
    assertStyle(
        matches, '--search-box-results-url-selected', 'rgba(0, 0, 12, 1)');
    assertStyle(matches, '--search-box-results-url', 'rgba(0, 0, 11, 1)');
  });

  test('realbox default loupe icon', async () => {
    // Arrange.
    loadTimeData.overrideValues({
      realboxDefaultIcon: 'search.svg',
    });
    document.body.innerHTML = '';
    realbox = document.createElement('ntp-realbox');
    document.body.appendChild(realbox);

    // Assert.
    assertIconMaskImageUrl(realbox.$.icon, 'search.svg');
  });

  test('realbox default Google G icon', async () => {
    // Arrange.
    loadTimeData.overrideValues({
      realboxDefaultIcon: 'realbox/icons/google_g.svg',
    });
    document.body.innerHTML = '';
    realbox = document.createElement('ntp-realbox');
    document.body.appendChild(realbox);

    // Assert.
    assertIconBackgroundImageUrl(realbox.$.icon, 'realbox/icons/google_g.svg');

    // Restore.
    loadTimeData.overrideValues({
      realboxDefaultIcon: 'search.svg',
    });
  });

  //============================================================================
  // Test Querying Autocomplete
  //============================================================================

  test('left-clicking empy input queries autocomplete', async () => {
    // Query zero-prefix matches.
    realbox.$.input.value = '';
    // Left click queries autocomplete when matches are not showing.
    realbox.$.input.dispatchEvent(new MouseEvent('mousedown', {button: 0}));
    await testProxy.handler.whenCalled('queryAutocomplete').then((args) => {
      assertEquals(decodeString16(args.input), realbox.$.input.value);
      assertFalse(args.preventInlineAutocomplete);
    });
    assertEquals(1, testProxy.handler.getCallCount('queryAutocomplete'));

    testProxy.handler.reset();

    // Show zero-prefix matches.
    const matches = [createSearchMatch(), createUrlMatch()];
    testProxy.callbackRouterRemote.autocompleteResultChanged({
      input: mojoString16(''),
      matches,
      suggestionGroupsMap: {},
    });
    await testProxy.callbackRouterRemote.$.flushForTesting();

    assertTrue(areMatchesShowing());
    const matchEls =
        realbox.$.matches.shadowRoot!.querySelectorAll('ntp-realbox-match');
    assertEquals(2, matchEls.length);

    // Left click does not query autocomplete when matches are showing.
    realbox.$.input.dispatchEvent(new MouseEvent('mousedown', {button: 0}));
    assertEquals(0, testProxy.handler.getCallCount('queryAutocomplete'));

    // Hide the matches by focusing out.
    matchEls[0]!.dispatchEvent(new FocusEvent('focusout', {
      bubbles: true,
      cancelable: true,
      composed: true,  // So it propagates across shadow DOM boundary.
      relatedTarget: document.body,
    }));

    // Right click does not query autocomplete.
    realbox.$.input.dispatchEvent(new MouseEvent('mousedown', {button: 1}));
    assertEquals(0, testProxy.handler.getCallCount('queryAutocomplete'));

    // Left click does not query autocomplete when input is non-empty.
    realbox.$.input.value = '   ';
    realbox.$.input.dispatchEvent(new MouseEvent('mousedown', {button: 0}));
    assertEquals(0, testProxy.handler.getCallCount('queryAutocomplete'));
  });

  test('focusing the input does not query autocomplete', async () => {
    realbox.$.input.value = '';
    realbox.$.input.focus();
    assertEquals(realbox.$.input, getDeepActiveElement());
    assertEquals(0, testProxy.handler.getCallCount('queryAutocomplete'));
  });

  test('tabbing into empty input queries autocomplete', async () => {
    // Query zero-prefix matches.
    realbox.$.input.value = '';
    realbox.$.input.dispatchEvent(new MouseEvent('mousedown', {button: 0}));
    await testProxy.handler.whenCalled('queryAutocomplete').then((args) => {
      assertEquals(decodeString16(args.input), realbox.$.input.value);
      assertFalse(args.preventInlineAutocomplete);
    });
    assertEquals(1, testProxy.handler.getCallCount('queryAutocomplete'));

    testProxy.handler.reset();

    // Show zero-prefix matches.
    const matches = [createSearchMatch(), createUrlMatch()];
    testProxy.callbackRouterRemote.autocompleteResultChanged({
      input: mojoString16(''),
      matches,
      suggestionGroupsMap: {},
    });
    await testProxy.callbackRouterRemote.$.flushForTesting();

    assertTrue(areMatchesShowing());
    const matchEls =
        realbox.$.matches.shadowRoot!.querySelectorAll('ntp-realbox-match');
    assertEquals(2, matchEls.length);

    // Tabbing into input does not query autocomplete when matches are showing.
    realbox.$.input.dispatchEvent(new KeyboardEvent('keyup', {
      bubbles: true,
      cancelable: true,
      key: 'Tab',
    }));
    assertEquals(0, testProxy.handler.getCallCount('queryAutocomplete'));

    // Hide the matches by focusing out.
    matchEls[0]!.dispatchEvent(new FocusEvent('focusout', {
      bubbles: true,
      cancelable: true,
      composed: true,  // So it propagates across shadow DOM boundary.
      relatedTarget: document.body,
    }));

    // Tabbing into empty input queries autocomplete.
    realbox.$.input.dispatchEvent(new KeyboardEvent('keyup', {
      bubbles: true,
      cancelable: true,
      key: 'Tab',
    }));
    await testProxy.handler.whenCalled('queryAutocomplete').then((args) => {
      assertEquals(decodeString16(args.input), realbox.$.input.value);
      assertFalse(args.preventInlineAutocomplete);
    });
    assertEquals(1, testProxy.handler.getCallCount('queryAutocomplete'));

    testProxy.handler.reset();

    // Tabbing into non-empty input does not query autocomplete.
    realbox.$.input.value = '   ';
    realbox.$.input.dispatchEvent(new KeyboardEvent('keyup', {
      bubbles: true,
      cancelable: true,
      key: 'Tab',
    }));
    assertEquals(0, testProxy.handler.getCallCount('queryAutocomplete'));
  });

  test('arrow up/down keys in empty input query autocomplete', async () => {
    // Query zero-prefix matches.
    realbox.$.input.value = '';
    realbox.$.input.dispatchEvent(new MouseEvent('mousedown', {button: 0}));
    await testProxy.handler.whenCalled('queryAutocomplete').then((args) => {
      assertEquals(decodeString16(args.input), realbox.$.input.value);
      assertFalse(args.preventInlineAutocomplete);
    });
    assertEquals(1, testProxy.handler.getCallCount('queryAutocomplete'));

    testProxy.handler.reset();

    // Show zero-prefix matches.
    const matches = [createSearchMatch(), createUrlMatch()];
    testProxy.callbackRouterRemote.autocompleteResultChanged({
      input: mojoString16(''),
      matches,
      suggestionGroupsMap: {},
    });
    await testProxy.callbackRouterRemote.$.flushForTesting();

    assertTrue(areMatchesShowing());
    const matchEls =
        realbox.$.matches.shadowRoot!.querySelectorAll('ntp-realbox-match');
    assertEquals(2, matchEls.length);

    // Arrow up/down keys do not query autocomplete when matches are showing.
    realbox.$.input.dispatchEvent(new KeyboardEvent('keydown', {
      bubbles: true,
      cancelable: true,
      key: 'ArrowUp',
    }));
    assertEquals(0, testProxy.handler.getCallCount('queryAutocomplete'));

    // Hide the matches by focusing out.
    matchEls[0]!.dispatchEvent(new FocusEvent('focusout', {
      bubbles: true,
      cancelable: true,
      composed: true,  // So it propagates across shadow DOM boundary.
      relatedTarget: document.body,
    }));

    // Arrow up/down keys query autocomplete.
    realbox.$.input.dispatchEvent(new KeyboardEvent('keydown', {
      bubbles: true,
      cancelable: true,
      key: 'ArrowDown',
    }));
    await testProxy.handler.whenCalled('queryAutocomplete').then((args) => {
      assertEquals(decodeString16(args.input), realbox.$.input.value);
      assertTrue(args.preventInlineAutocomplete);
    });
    assertEquals(1, testProxy.handler.getCallCount('queryAutocomplete'));
  });

  test('arrow up/down keys in non-empty input query autocomplete', async () => {
    // Query matches.
    realbox.$.input.value = 'hello';
    realbox.$.input.dispatchEvent(new InputEvent('input'));
    await testProxy.handler.whenCalled('queryAutocomplete').then((args) => {
      assertEquals(decodeString16(args.input), realbox.$.input.value);
      assertFalse(args.preventInlineAutocomplete);
    });
    assertEquals(1, testProxy.handler.getCallCount('queryAutocomplete'));

    testProxy.handler.reset();

    // Show matches.
    const matches = [createSearchMatch(), createUrlMatch()];
    testProxy.callbackRouterRemote.autocompleteResultChanged({
      input: mojoString16('hello'),
      matches,
      suggestionGroupsMap: {},
    });
    await testProxy.callbackRouterRemote.$.flushForTesting();

    assertTrue(areMatchesShowing());
    const matchEls =
        realbox.$.matches.shadowRoot!.querySelectorAll('ntp-realbox-match');
    assertEquals(2, matchEls.length);

    // Arrow up/down keys do not query autocomplete when matches are showing.
    realbox.$.input.dispatchEvent(new KeyboardEvent('keydown', {
      bubbles: true,
      cancelable: true,
      key: 'ArrowUp',
    }));
    assertEquals(0, testProxy.handler.getCallCount('queryAutocomplete'));

    // Hide the matches by focusing out.
    matchEls[0]!.dispatchEvent(new FocusEvent('focusout', {
      bubbles: true,
      cancelable: true,
      composed: true,  // So it propagates across shadow DOM boundary.
      relatedTarget: document.body,
    }));

    // Arrow up/down keys query autocomplete.
    realbox.$.input.dispatchEvent(new KeyboardEvent('keydown', {
      bubbles: true,
      cancelable: true,
      key: 'ArrowDown',
    }));
    await testProxy.handler.whenCalled('queryAutocomplete').then((args) => {
      assertEquals(decodeString16(args.input), realbox.$.input.value);
      assertFalse(args.preventInlineAutocomplete);
    });
    assertEquals(1, testProxy.handler.getCallCount('queryAutocomplete'));
  });

  test('empty input does not query autocomplete', async () => {
    realbox.$.input.value = '';
    realbox.$.input.dispatchEvent(new InputEvent('input'));
    assertEquals(0, testProxy.handler.getCallCount('queryAutocomplete'));
  });

  test('typing space does not query autocomplete', async () => {
    realbox.$.input.value = ' ';
    realbox.$.input.dispatchEvent(new InputEvent('input'));
    assertEquals(0, testProxy.handler.getCallCount('queryAutocomplete'));
  });

  test('typing queries autocomplete', async () => {
    realbox.$.input.value = 'he';
    realbox.$.input.dispatchEvent(new InputEvent('input'));

    await testProxy.handler.whenCalled('queryAutocomplete').then((args) => {
      assertEquals(decodeString16(args.input), realbox.$.input.value);
      assertFalse(args.preventInlineAutocomplete);
    });
    assertEquals(1, testProxy.handler.getCallCount('queryAutocomplete'));

    testProxy.handler.reset();

    // Deleting text from input prevents inline autocompletion.
    realbox.$.input.value = 'h';
    realbox.$.input.dispatchEvent(new InputEvent('input'));

    await testProxy.handler.whenCalled('queryAutocomplete').then((args) => {
      assertEquals(decodeString16(args.input), realbox.$.input.value);
      assertTrue(args.preventInlineAutocomplete);
    });
    assertEquals(1, testProxy.handler.getCallCount('queryAutocomplete'));

    testProxy.handler.reset();

    realbox.$.input.value = 'he';
    realbox.$.input.dispatchEvent(new InputEvent('input'));

    await testProxy.handler.whenCalled('queryAutocomplete').then((args) => {
      assertEquals(decodeString16(args.input), realbox.$.input.value);
      assertFalse(args.preventInlineAutocomplete);
    });
    assertEquals(1, testProxy.handler.getCallCount('queryAutocomplete'));

    testProxy.handler.reset();

    // Pasting text into the input prevents inline autocompletion.
    const pasteEvent = createClipboardEvent('paste');
    realbox.$.input.dispatchEvent(pasteEvent);
    assertFalse(pasteEvent.defaultPrevented);
    realbox.$.input.value = 'hel';
    realbox.$.input.dispatchEvent(new InputEvent('input'));

    await testProxy.handler.whenCalled('queryAutocomplete').then((args) => {
      assertEquals(decodeString16(args.input), realbox.$.input.value);
      assertTrue(args.preventInlineAutocomplete);
    });
    assertEquals(1, testProxy.handler.getCallCount('queryAutocomplete'));

    testProxy.handler.reset();

    realbox.$.input.value = 'hell';
    realbox.$.input.dispatchEvent(new InputEvent('input'));

    await testProxy.handler.whenCalled('queryAutocomplete').then((args) => {
      assertEquals(decodeString16(args.input), realbox.$.input.value);
      assertFalse(args.preventInlineAutocomplete);
    });
    assertEquals(1, testProxy.handler.getCallCount('queryAutocomplete'));

    testProxy.handler.reset();

    // If caret isn't at the end of the text inline autocompletion is prevented.
    realbox.$.input.value = 'hello';
    realbox.$.input.setSelectionRange(0, 0);  // Move caret to beginning.
    realbox.$.input.dispatchEvent(new InputEvent('input'));

    await testProxy.handler.whenCalled('queryAutocomplete').then((args) => {
      assertEquals(decodeString16(args.input), realbox.$.input.value);
      assertTrue(args.preventInlineAutocomplete);
    });
    assertEquals(1, testProxy.handler.getCallCount('queryAutocomplete'));

    testProxy.handler.reset();

    // If text is being composed with an IME inline autocompletion is prevented.
    realbox.$.input.value = 'hello 간';
    const inputEvent = new InputEvent('input', {isComposing: true});
    realbox.$.input.dispatchEvent(inputEvent);

    await testProxy.handler.whenCalled('queryAutocomplete').then((args) => {
      assertEquals(decodeString16(args.input), realbox.$.input.value);
      assertTrue(args.preventInlineAutocomplete);
    });
    assertEquals(1, testProxy.handler.getCallCount('queryAutocomplete'));

    testProxy.handler.reset();
  });

  test('clearing the input stops autocomplete', async () => {
    realbox.$.input.value = 'h';
    realbox.$.input.dispatchEvent(new InputEvent('input'));

    await testProxy.handler.whenCalled('queryAutocomplete').then((args) => {
      assertEquals(decodeString16(args.input), realbox.$.input.value);
      assertFalse(args.preventInlineAutocomplete);
    });
    assertEquals(1, testProxy.handler.getCallCount('queryAutocomplete'));

    realbox.$.input.value = '';
    realbox.$.input.dispatchEvent(new InputEvent('input'));
    await testProxy.handler.whenCalled('stopAutocomplete').then((args) => {
      assertTrue(args.clearResult);
    });
  });

  //============================================================================
  // Test Autocomplete Response
  //============================================================================

  test('autocomplete response', async () => {
    realbox.$.input.value = '      hello world';
    realbox.$.input.dispatchEvent(new InputEvent('input'));
    await testProxy.handler.whenCalled('queryAutocomplete').then((args) => {
      assertEquals(decodeString16(args.input), realbox.$.input.value);
      assertFalse(args.preventInlineAutocomplete);
    });
    assertEquals(1, testProxy.handler.getCallCount('queryAutocomplete'));

    const matches = [
      createSearchMatch({
        allowedToBeDefaultMatch: true,
      }),
      createUrlMatch(),
    ];
    testProxy.callbackRouterRemote.autocompleteResultChanged({
      input: mojoString16(realbox.$.input.value.trimLeft()),
      matches,
      suggestionGroupsMap: {},
    });
    await testProxy.callbackRouterRemote.$.flushForTesting();

    assertTrue(areMatchesShowing());
    assertEquals('listbox', realbox.$.matches.getAttribute('role'));
    const matchEls =
        realbox.$.matches.shadowRoot!.querySelectorAll('ntp-realbox-match');
    assertEquals(2, matchEls.length);
    verifyMatch(matches[0]!, matchEls[0]!);
    verifyMatch(matches[1]!, matchEls[1]!);

    // First match is selected.
    assertTrue(matchEls[0]!.classList.contains(Classes.SELECTED));

    assertEquals('      hello world', realbox.$.input.value);
    const start = realbox.$.input.selectionStart!;
    const end = realbox.$.input.selectionEnd!;
    assertEquals('', realbox.$.input.value.substring(start, end));
  });

  test('autocomplete response with inline autocompletion', async () => {
    realbox.$.input.value = 'hello ';
    realbox.$.input.dispatchEvent(new InputEvent('input'));
    await testProxy.handler.whenCalled('queryAutocomplete').then((args) => {
      assertEquals(decodeString16(args.input), realbox.$.input.value);
      assertFalse(args.preventInlineAutocomplete);
    });
    assertEquals(1, testProxy.handler.getCallCount('queryAutocomplete'));

    testProxy.handler.reset();

    const matches = [createSearchMatch({
      allowedToBeDefaultMatch: true,
      inlineAutocompletion: mojoString16('world'),
    })];
    testProxy.callbackRouterRemote.autocompleteResultChanged({
      input: mojoString16(realbox.$.input.value.trimLeft()),
      matches,
      suggestionGroupsMap: {},
    });
    await testProxy.callbackRouterRemote.$.flushForTesting();

    assertTrue(areMatchesShowing());

    const matchEls =
        realbox.$.matches.shadowRoot!.querySelectorAll('ntp-realbox-match');
    assertEquals(1, matchEls.length);
    verifyMatch(matches[0]!, matchEls[0]!);

    // First match is selected.
    assertTrue(matchEls[0]!.classList.contains(Classes.SELECTED));

    assertEquals('hello world', realbox.$.input.value);
    let start = realbox.$.input.selectionStart!;
    let end = realbox.$.input.selectionEnd!;
    assertEquals('world', realbox.$.input.value.substring(start, end));

    // Define a new |value| property on the input to see whether it gets set.
    let inputValueChanged = false;
    const originalValueProperty =
        Object.getOwnPropertyDescriptor(HTMLInputElement.prototype, 'value')!;
    Object.defineProperty(realbox.$.input, 'value', {
      get: originalValueProperty.get,
      set: (value) => {
        inputValueChanged = true;
        originalValueProperty.set!.call(realbox.$.input, value);
      },
    });

    // If the user types the next character of the inline autocompletion, the
    // keydown event is stopped, inline autocompletion is moved forward and
    // autocomplete is queried with the non inline autocompletion part of input.
    const keyEvent = new KeyboardEvent('keydown', {
      bubbles: true,
      cancelable: true,
      composed: true,  // So it propagates across shadow DOM boundary.
      key: 'w',
    });
    realbox.$.input.dispatchEvent(keyEvent);
    assertTrue(keyEvent.defaultPrevented);

    assertFalse(inputValueChanged);
    assertEquals('hello world', realbox.$.input.value);
    start = realbox.$.input.selectionStart!;
    end = realbox.$.input.selectionEnd!;
    assertEquals('orld', realbox.$.input.value.substring(start, end));

    await testProxy.handler.whenCalled('queryAutocomplete').then((args) => {
      assertEquals(decodeString16(args.input), 'hello w');
      assertFalse(args.preventInlineAutocomplete);
    });
    assertEquals(1, testProxy.handler.getCallCount('queryAutocomplete'));
  });

  test('autocomplete response perserves cursor position', async () => {
    realbox.$.input.value = 'hello';
    realbox.$.input.selectionStart = 0;
    realbox.$.input.selectionEnd = 4;
    realbox.$.input.dispatchEvent(new InputEvent('input'));

    const matches = [createSearchMatch({
      allowedToBeDefaultMatch: true,
      contents: mojoString16('hello'),
    })];
    testProxy.callbackRouterRemote.autocompleteResultChanged({
      input: mojoString16(realbox.$.input.value.trimLeft()),
      matches,
      suggestionGroupsMap: {},
    });
    await testProxy.callbackRouterRemote.$.flushForTesting();

    assertEquals('hello', realbox.$.input.value);
    const start = realbox.$.input.selectionStart!;
    const end = realbox.$.input.selectionEnd!;
    assertEquals('hell', realbox.$.input.value.substring(start, end));
  });

  test('stale autocomplete response is ignored', async () => {
    realbox.$.input.value = 'he';
    realbox.$.input.dispatchEvent(new InputEvent('input'));

    const matches = [createSearchMatch(), createUrlMatch()];
    testProxy.callbackRouterRemote.autocompleteResultChanged({
      input: mojoString16('h'),  // Simulate stale response.
      matches,
      suggestionGroupsMap: {},
    });
    await testProxy.callbackRouterRemote.$.flushForTesting();

    assertFalse(areMatchesShowing());
    const matchEls =
        realbox.$.matches.shadowRoot!.querySelectorAll('ntp-realbox-match');
    assertEquals(0, matchEls.length);
  });

  test('autocomplete response changes', async () => {
    realbox.$.input.value = 'he';
    realbox.$.input.dispatchEvent(new InputEvent('input'));

    const matches = [createSearchMatch(), createUrlMatch()];
    testProxy.callbackRouterRemote.autocompleteResultChanged({
      input: mojoString16(realbox.$.input.value.trimLeft()),
      matches,
      suggestionGroupsMap: {},
    });
    await testProxy.callbackRouterRemote.$.flushForTesting();

    assertTrue(areMatchesShowing());
    let matchEls =
        realbox.$.matches.shadowRoot!.querySelectorAll('ntp-realbox-match');
    assertEquals(2, matchEls.length);

    realbox.$.input.value += 'll';
    realbox.$.input.dispatchEvent(new InputEvent('input'));

    testProxy.callbackRouterRemote.autocompleteResultChanged({
      input: mojoString16(realbox.$.input.value.trimLeft()),
      matches: [],
      suggestionGroupsMap: {},
    });
    await testProxy.callbackRouterRemote.$.flushForTesting();

    assertFalse(areMatchesShowing());
    matchEls =
        realbox.$.matches.shadowRoot!.querySelectorAll('ntp-realbox-match');
    assertEquals(0, matchEls.length);

    realbox.$.input.value += 'o';
    realbox.$.input.dispatchEvent(new InputEvent('input'));

    testProxy.callbackRouterRemote.autocompleteResultChanged({
      input: mojoString16(realbox.$.input.value.trimLeft()),
      matches,
      suggestionGroupsMap: {},
    });
    await testProxy.callbackRouterRemote.$.flushForTesting();

    assertTrue(areMatchesShowing());
    matchEls =
        realbox.$.matches.shadowRoot!.querySelectorAll('ntp-realbox-match');
    assertEquals(2, matchEls.length);
  });

  //============================================================================
  // Test Cut/Copy
  //============================================================================

  test('Copying or cutting empty input fails', async () => {
    realbox.$.input.value = '';

    const copyEvent = createClipboardEvent('copy');
    realbox.$.input.dispatchEvent(copyEvent);
    assertFalse(copyEvent.defaultPrevented);

    const cutEvent = createClipboardEvent('cut');
    realbox.$.input.dispatchEvent(cutEvent);
    assertFalse(cutEvent.defaultPrevented);
  });

  test('Copying or cutting search match fails', async () => {
    realbox.$.input.value = 'hello ';
    realbox.$.input.dispatchEvent(new InputEvent('input'));

    const matches = [createSearchMatch({
      allowedToBeDefaultMatch: true,
      inlineAutocompletion: mojoString16('world'),
    })];
    testProxy.callbackRouterRemote.autocompleteResultChanged({
      input: mojoString16(realbox.$.input.value.trimLeft()),
      matches,
      suggestionGroupsMap: {},
    });
    await testProxy.callbackRouterRemote.$.flushForTesting();

    assertEquals('hello world', realbox.$.input.value);
    const start = realbox.$.input.selectionStart!;
    const end = realbox.$.input.selectionEnd!;
    assertEquals('world', realbox.$.input.value.substring(start, end));

    // Select the entire input.
    realbox.$.input.setSelectionRange(0, realbox.$.input.value.length);

    const copyEvent = createClipboardEvent('copy');
    realbox.$.input.dispatchEvent(copyEvent);
    assertFalse(copyEvent.defaultPrevented);

    const cutEvent = createClipboardEvent('cut');
    realbox.$.input.dispatchEvent(cutEvent);
    assertFalse(cutEvent.defaultPrevented);
  });

  test('Copying or cutting URL match succeeds', async () => {
    realbox.$.input.value = 'hello';
    realbox.$.input.dispatchEvent(new InputEvent('input'));

    const matches = [createUrlMatch({
      allowedToBeDefaultMatch: true,
      inlineAutocompletion: mojoString16('world.com'),
    })];
    testProxy.callbackRouterRemote.autocompleteResultChanged({
      input: mojoString16(realbox.$.input.value.trimLeft()),
      matches,
      suggestionGroupsMap: {},
    });
    await testProxy.callbackRouterRemote.$.flushForTesting();

    assertEquals('helloworld.com', realbox.$.input.value);
    const start = realbox.$.input.selectionStart!;
    const end = realbox.$.input.selectionEnd!;
    assertEquals('world.com', realbox.$.input.value.substring(start, end));

    const copyEvent = createClipboardEvent('copy');
    realbox.$.input.dispatchEvent(copyEvent);
    assertFalse(copyEvent.defaultPrevented);

    const cutEvent = createClipboardEvent('cut');
    realbox.$.input.dispatchEvent(cutEvent);
    assertFalse(cutEvent.defaultPrevented);

    // Select the entire input.
    realbox.$.input.setSelectionRange(0, realbox.$.input.value.length);

    realbox.$.input.dispatchEvent(copyEvent);
    assertTrue(copyEvent.defaultPrevented);
    assertEquals(
        'https://helloworld.com/',
        copyEvent.clipboardData!.getData('text/plain'));

    realbox.$.input.dispatchEvent(cutEvent);
    assertTrue(cutEvent.defaultPrevented);
    assertEquals(
        'https://helloworld.com/',
        cutEvent.clipboardData!.getData('text/plain'));
  });

  //============================================================================
  // Test Navigation
  //============================================================================

  test('pressing Enter on input navigates to the selected match', async () => {
    // Input is expected to have been focused before any navigation.
    realbox.$.input.dispatchEvent(new Event('focus'));

    realbox.$.input.value = '  hello  ';
    realbox.$.input.dispatchEvent(new InputEvent('input'));

    const matches = [
      createSearchMatch({
        allowedToBeDefaultMatch: true,
      }),
      createUrlMatch(),
    ];
    testProxy.callbackRouterRemote.autocompleteResultChanged({
      input: mojoString16(realbox.$.input.value.trimLeft()),
      matches,
      suggestionGroupsMap: {},
    });
    await testProxy.callbackRouterRemote.$.flushForTesting();

    assertTrue(areMatchesShowing());
    const matchEls =
        realbox.$.matches.shadowRoot!.querySelectorAll('ntp-realbox-match');
    assertEquals(2, matchEls.length);

    // First match is selected.
    assertTrue(matchEls[0]!.classList.contains(Classes.SELECTED));

    const shiftEnter = new KeyboardEvent('keydown', {
      bubbles: true,
      cancelable: true,
      composed: true,  // So it propagates across shadow DOM boundary.
      key: 'Enter',
      shiftKey: true,
    });
    realbox.$.input.dispatchEvent(shiftEnter);
    assertTrue(shiftEnter.defaultPrevented);

    // Navigates to the first match.
    await testProxy.handler.whenCalled('openAutocompleteMatch').then((args) => {
      assertEquals(0, args.line);
      assertEquals(matches[0]!.destinationUrl.url, args.url.url);
      assertTrue(args.areMatchesShowing);
      assertTrue(args.shiftKey);
      assertTrue(args.timeElapsedSinceLastFocus.microseconds > 0);
    });
    assertEquals(1, testProxy.handler.getCallCount('openAutocompleteMatch'));
  });

  test(
      'pressing Enter on input navigates to *hidden* selected match',
      async () => {
        // Input is expected to have been focused before any navigation.
        realbox.$.input.dispatchEvent(new Event('focus'));

        realbox.$.input.value = '  hello  ';
        realbox.$.input.dispatchEvent(new InputEvent('input'));

        const matches =
            [createSearchMatch({iconUrl: 'clock.svg'}), createUrlMatch()];
        testProxy.callbackRouterRemote.autocompleteResultChanged({
          input: mojoString16(realbox.$.input.value.trimLeft()),
          matches,
          suggestionGroupsMap: {},
        });
        await testProxy.callbackRouterRemote.$.flushForTesting();

        assertTrue(areMatchesShowing());
        let matchEls =
            realbox.$.matches.shadowRoot!.querySelectorAll('ntp-realbox-match');
        assertEquals(2, matchEls.length);

        // Select the first match.
        matchEls[0]!.dispatchEvent(new Event('focusin', {
          bubbles: true,
          cancelable: true,
          composed: true,  // So it propagates across shadow DOM boundary.
        }));

        // First match is selected.
        assertTrue(matchEls[0]!.classList.contains(Classes.SELECTED));
        // Input is updated.
        assertEquals('hello world', realbox.$.input.value);
        // Icon is updated.
        assertIconMaskImageUrl(realbox.$.icon, 'clock.svg');

        // Hide the matches by focusing out.
        matchEls[0]!.dispatchEvent(new FocusEvent('focusout', {
          bubbles: true,
          cancelable: true,
          composed: true,  // So it propagates across shadow DOM boundary.
          relatedTarget: document.body,
        }));

        // Matches are hidden.
        assertFalse(areMatchesShowing());
        // Force a synchronous render.
        realbox.$.matches.$.groups.render();
        // First match is still selected.
        matchEls =
            realbox.$.matches.shadowRoot!.querySelectorAll('ntp-realbox-match');
        assertEquals(2, matchEls.length);
        assertTrue(matchEls[0]!.classList.contains(Classes.SELECTED));
        // Input is not cleared.
        assertEquals('hello world', realbox.$.input.value);
        // Icon is not cleared.
        assertIconMaskImageUrl(realbox.$.icon, 'clock.svg');

        const shiftEnter = new KeyboardEvent('keydown', {
          bubbles: true,
          cancelable: true,
          key: 'Enter',
          composed: true,  // So it propagates across shadow DOM boundary.
          shiftKey: true,
        });
        realbox.$.input.dispatchEvent(shiftEnter);
        assertTrue(shiftEnter.defaultPrevented);

        // Navigates to the first match.
        await testProxy.handler.whenCalled('openAutocompleteMatch')
            .then((args) => {
              assertEquals(0, args.line);
              assertEquals(matches[0]!.destinationUrl.url, args.url.url);
              assertFalse(args.areMatchesShowing);
              assertTrue(args.shiftKey);
              assertTrue(args.timeElapsedSinceLastFocus.microseconds > 0);
            });
        assertEquals(
            1, testProxy.handler.getCallCount('openAutocompleteMatch'));
      });

  test('pressing Enter on input is ignored if no selected match', async () => {
    // Input is expected to have been focused before any navigation.
    realbox.$.input.dispatchEvent(new Event('focus'));

    realbox.$.input.value = 'hello';
    realbox.$.input.dispatchEvent(new InputEvent('input'));

    const matches = [createSearchMatch(), createUrlMatch()];
    testProxy.callbackRouterRemote.autocompleteResultChanged({
      input: mojoString16(realbox.$.input.value.trimLeft()),
      matches,
      suggestionGroupsMap: {},
    });
    await testProxy.callbackRouterRemote.$.flushForTesting();

    assertTrue(areMatchesShowing());
    const matchEls =
        realbox.$.matches.shadowRoot!.querySelectorAll('ntp-realbox-match');
    assertEquals(2, matchEls.length);

    // First match is not selected.
    assertFalse(matchEls[0]!.classList.contains(Classes.SELECTED));

    const shiftEnter = new KeyboardEvent('keydown', {
      bubbles: true,
      cancelable: true,
      composed: true,  // So it propagates across shadow DOM boundary.
      key: 'Enter',
      shiftKey: true,
    });
    realbox.$.input.dispatchEvent(shiftEnter);
    assertFalse(shiftEnter.defaultPrevented);

    // Did not navigate to the first match since it's not selected.
    assertEquals(0, testProxy.handler.getCallCount('openAutocompleteMatch'));
  });

  test(
      'pressing Enter on input is ignored if no *hidden* selected match',
      async () => {
        realbox.$.input.value = '';
        realbox.$.input.dispatchEvent(new MouseEvent('mousedown', {button: 0}));

        const matches =
            [createSearchMatch({iconUrl: 'clock.svg'}), createUrlMatch()];
        testProxy.callbackRouterRemote.autocompleteResultChanged({
          input: mojoString16(realbox.$.input.value.trimLeft()),
          matches,
          suggestionGroupsMap: {},
        });
        await testProxy.callbackRouterRemote.$.flushForTesting();

        assertTrue(areMatchesShowing());
        let matchEls =
            realbox.$.matches.shadowRoot!.querySelectorAll('ntp-realbox-match');
        assertEquals(2, matchEls.length);

        // Select the first match.
        matchEls[0]!.dispatchEvent(new Event('focusin', {
          bubbles: true,
          cancelable: true,
          composed: true,  // So it propagates across shadow DOM boundary.
        }));

        // First match is selected.
        assertTrue(matchEls[0]!.classList.contains(Classes.SELECTED));
        // Input is updated.
        assertEquals('hello world', realbox.$.input.value);
        // Icon is updated.
        assertIconMaskImageUrl(realbox.$.icon, 'clock.svg');

        // Hide the matches by focusing out.
        matchEls[0]!.dispatchEvent(new FocusEvent('focusout', {
          bubbles: true,
          cancelable: true,
          composed: true,  // So it propagates across shadow DOM boundary.
          relatedTarget: document.body,
        }));

        // Matches are hidden.
        assertFalse(areMatchesShowing());
        // Force a synchronous render.
        realbox.$.matches.$.groups.render();
        // Matches are cleared.
        matchEls =
            realbox.$.matches.shadowRoot!.querySelectorAll('ntp-realbox-match');
        assertEquals(0, matchEls.length);
        // Input is cleared (zero-prefix case).
        assertEquals('', realbox.$.input.value);
        // Icon is restored (zero-prefix case).
        assertIconMaskImageUrl(realbox.$.icon, 'search.svg');

        const shiftEnter = new KeyboardEvent('keydown', {
          bubbles: true,
          cancelable: true,
          composed: true,  // So it propagates across shadow DOM boundary.
          key: 'Enter',
          shiftKey: true,
        });
        realbox.$.input.dispatchEvent(shiftEnter);
        assertFalse(shiftEnter.defaultPrevented);

        // Did not navigate to the first match since it's not selected.
        assertEquals(
            0, testProxy.handler.getCallCount('openAutocompleteMatch'));
      });

  test('pressing Enter on input too quickly', async () => {
    // Input is expected to have been focused before any navigation.
    realbox.$.input.dispatchEvent(new Event('focus'));

    realbox.$.input.value = 'hello';
    realbox.$.input.dispatchEvent(new InputEvent('input'));

    const matches = [
      createSearchMatch({
        allowedToBeDefaultMatch: true,
      }),
      createUrlMatch(),
    ];
    testProxy.callbackRouterRemote.autocompleteResultChanged({
      input: mojoString16(realbox.$.input.value.trimLeft()),
      matches,
      suggestionGroupsMap: {},
    });
    await testProxy.callbackRouterRemote.$.flushForTesting();

    assertTrue(areMatchesShowing());
    const matchEls =
        realbox.$.matches.shadowRoot!.querySelectorAll('ntp-realbox-match');
    assertEquals(2, matchEls.length);

    // First match is selected.
    assertTrue(matchEls[0]!.classList.contains(Classes.SELECTED));

    // User types some more and presses Enter before the results update.
    realbox.$.input.value = 'hello world';
    realbox.$.input.dispatchEvent(new InputEvent('input'));

    const shiftEnter = new KeyboardEvent('keydown', {
      bubbles: true,
      cancelable: true,
      composed: true,  // So it propagates across shadow DOM boundary.
      key: 'Enter',
      shiftKey: true,
    });
    realbox.$.input.dispatchEvent(shiftEnter);
    assertTrue(shiftEnter.defaultPrevented);

    // Did not navigate to the first match since it's stale.
    assertEquals(0, testProxy.handler.getCallCount('openAutocompleteMatch'));

    // New matches arrive.
    testProxy.callbackRouterRemote.autocompleteResultChanged({
      input: mojoString16(realbox.$.input.value.trimLeft()),
      matches,
      suggestionGroupsMap: {},
    });
    await testProxy.callbackRouterRemote.$.flushForTesting();

    // First match is selected.
    assertTrue(matchEls[0]!.classList.contains(Classes.SELECTED));

    // Navigates to the first match immediately without further user action.
    await testProxy.handler.whenCalled('openAutocompleteMatch').then((args) => {
      assertEquals(0, args.line);
      assertEquals(matches[0]!.destinationUrl.url, args.url.url);
      assertTrue(args.areMatchesShowing);
      assertTrue(args.shiftKey);
      assertTrue(args.timeElapsedSinceLastFocus.microseconds > 0);
    });
    assertEquals(1, testProxy.handler.getCallCount('openAutocompleteMatch'));
  });

  test('pressing Enter on the selected match navigates to it', async () => {
    // Input is expected to have been focused before any navigation.
    realbox.$.input.dispatchEvent(new Event('focus'));

    realbox.$.input.value = 'hello';
    realbox.$.input.dispatchEvent(new InputEvent('input'));

    const matches = [
      createSearchMatch({
        allowedToBeDefaultMatch: true,
      }),
      createUrlMatch(),
    ];
    testProxy.callbackRouterRemote.autocompleteResultChanged({
      input: mojoString16(realbox.$.input.value.trimLeft()),
      matches,
      suggestionGroupsMap: {},
    });
    await testProxy.callbackRouterRemote.$.flushForTesting();

    assertTrue(areMatchesShowing());
    const matchEls =
        realbox.$.matches.shadowRoot!.querySelectorAll('ntp-realbox-match');
    assertEquals(2, matchEls.length);

    // First match is selected.
    assertTrue(matchEls[0]!.classList.contains(Classes.SELECTED));

    const shiftEnter = new KeyboardEvent('keydown', {
      bubbles: true,
      cancelable: true,
      composed: true,  // So it propagates across shadow DOM boundary.
      key: 'Enter',
      shiftKey: true,
    });
    matchEls[0]!.dispatchEvent(shiftEnter);
    assertTrue(shiftEnter.defaultPrevented);

    // Navigates to the first match is selected.
    await testProxy.handler.whenCalled('openAutocompleteMatch').then((args) => {
      assertEquals(0, args.line);
      assertEquals(matches[0]!.destinationUrl.url, args.url.url);
      assertTrue(args.areMatchesShowing);
      assertTrue(args.shiftKey);
      assertTrue(args.timeElapsedSinceLastFocus.microseconds > 0);
    });
    assertEquals(1, testProxy.handler.getCallCount('openAutocompleteMatch'));
  });

  test('Clicking a match navigates to it', async () => {
    // Input is expected to have been focused before any navigation.
    realbox.$.input.dispatchEvent(new Event('focus'));

    realbox.$.input.value = 'hello';
    realbox.$.input.dispatchEvent(new InputEvent('input'));

    const matches = [createSearchMatch(), createUrlMatch()];
    testProxy.callbackRouterRemote.autocompleteResultChanged({
      input: mojoString16(realbox.$.input.value.trimLeft()),
      matches,
      suggestionGroupsMap: {},
    });
    await testProxy.callbackRouterRemote.$.flushForTesting();

    assertTrue(areMatchesShowing());
    const matchEls =
        realbox.$.matches.shadowRoot!.querySelectorAll('ntp-realbox-match');
    assertEquals(2, matchEls.length);

    // Right clicks are ignored.
    const rightClick = new MouseEvent('click', {
      bubbles: true,
      button: 2,
      cancelable: true,
      composed: true,  // So it propagates across shadow DOM boundary.
    });
    matchEls[0]!.dispatchEvent(rightClick);
    assertFalse(rightClick.defaultPrevented);
    assertEquals(0, testProxy.handler.getCallCount('openAutocompleteMatch'));

    // Middle clicks are accepted.
    const middleClick = new MouseEvent('click', {
      bubbles: true,
      button: 1,
      cancelable: true,
      composed: true,  // So it propagates across shadow DOM boundary.
    });
    matchEls[0]!.dispatchEvent(middleClick);
    assertTrue(middleClick.defaultPrevented);

    await testProxy.handler.whenCalled('openAutocompleteMatch').then((args) => {
      assertEquals(0, args.line);
      assertEquals(matches[0]!.destinationUrl.url, args.url.url);
      assertTrue(args.areMatchesShowing);
      assertEquals(1, args.mouseButton);
      assertTrue(args.timeElapsedSinceLastFocus.microseconds > 0);
    });
    assertEquals(1, testProxy.handler.getCallCount('openAutocompleteMatch'));

    testProxy.handler.reset();

    // Left clicks are accepted.
    const leftClick = new MouseEvent('click', {
      bubbles: true,
      button: 0,
      cancelable: true,
      composed: true,  // So it propagates across shadow DOM boundary.
    });
    matchEls[0]!.dispatchEvent(leftClick);
    assertTrue(leftClick.defaultPrevented);

    await testProxy.handler.whenCalled('openAutocompleteMatch').then((args) => {
      assertEquals(0, args.line);
      assertEquals(matches[0]!.destinationUrl.url, args.url.url);
      assertTrue(args.areMatchesShowing);
      assertEquals(0, args.mouseButton);
      assertTrue(args.timeElapsedSinceLastFocus.microseconds > 0);
    });
    assertEquals(1, testProxy.handler.getCallCount('openAutocompleteMatch'));
  });

  //============================================================================
  // Test Deletion
  //============================================================================

  test('Remove button is visible if the match supports deletion', async () => {
    realbox.$.input.value = 'hello';
    realbox.$.input.dispatchEvent(new InputEvent('input'));

    const matches =
        [createSearchMatch(), createUrlMatch({supportsDeletion: true})];
    testProxy.callbackRouterRemote.autocompleteResultChanged({
      input: mojoString16(realbox.$.input.value.trimLeft()),
      matches,
      suggestionGroupsMap: {},
    });
    await testProxy.callbackRouterRemote.$.flushForTesting();

    assertTrue(areMatchesShowing());
    const matchEls =
        realbox.$.matches.shadowRoot!.querySelectorAll('ntp-realbox-match');
    assertEquals(2, matchEls.length);

    assertEquals(
        window.getComputedStyle(matchEls[0]!.$.remove).display, 'none');
    assertNotEquals(
        window.getComputedStyle(matchEls[1]!.$.remove).display, 'none');
  });

  test('Can remove match using the remove button', async () => {
    realbox.$.input.value = 'hello';
    realbox.$.input.dispatchEvent(new InputEvent('input'));

    const matches =
        [createSearchMatch(), createUrlMatch({supportsDeletion: true})];
    testProxy.callbackRouterRemote.autocompleteResultChanged({
      input: mojoString16(realbox.$.input.value.trimLeft()),
      matches,
      suggestionGroupsMap: {},
    });
    await testProxy.callbackRouterRemote.$.flushForTesting();

    assertTrue(areMatchesShowing());
    const matchEls =
        realbox.$.matches.shadowRoot!.querySelectorAll('ntp-realbox-match');
    assertEquals(2, matchEls.length);

    // Select the second match.
    const arrowUpEvent = new KeyboardEvent('keydown', {
      bubbles: true,
      cancelable: true,
      composed: true,  // So it propagates across shadow DOM boundary.
      key: 'ArrowUp',
    });
    realbox.$.input.dispatchEvent(arrowUpEvent);
    assertTrue(arrowUpEvent.defaultPrevented);
    assertTrue(matchEls[1]!.classList.contains(Classes.SELECTED));

    // By pressing 'Enter' on the button.
    const enter = new KeyboardEvent('keydown', {
      bubbles: true,
      cancelable: true,
      composed: true,  // So it propagates across shadow DOM boundary.
      key: 'Enter',
    });
    matchEls[1]!.$.remove.dispatchEvent(enter);
    assertTrue(enter.defaultPrevented);
    await testProxy.handler.whenCalled('deleteAutocompleteMatch')
        .then((args) => {
          assertEquals(1, args.line);
        });
    assertEquals(1, testProxy.handler.getCallCount('deleteAutocompleteMatch'));
    // Pressing 'Enter' on the button doesn't accidentally trigger navigation.
    assertEquals(0, testProxy.handler.getCallCount('openAutocompleteMatch'));

    testProxy.handler.reset();

    // By clicking the button.
    matchEls[1]!.$.remove.click();
    await testProxy.handler.whenCalled('deleteAutocompleteMatch')
        .then((args) => {
          assertEquals(1, args.line);
        });
    assertEquals(1, testProxy.handler.getCallCount('deleteAutocompleteMatch'));
    // Clicking the button doesn't accidentally trigger navigation.
    assertEquals(0, testProxy.handler.getCallCount('openAutocompleteMatch'));
  });

  test('Can remove selected match using keyboard shortcut', async () => {
    realbox.$.input.value = 'hello';
    realbox.$.input.dispatchEvent(new InputEvent('input'));

    const matches = [
      createSearchMatch({
        allowedToBeDefaultMatch: true,
      }),
      createUrlMatch({supportsDeletion: true}),
    ];
    testProxy.callbackRouterRemote.autocompleteResultChanged({
      input: mojoString16(realbox.$.input.value.trimLeft()),
      matches,
      suggestionGroupsMap: {},
    });
    await testProxy.callbackRouterRemote.$.flushForTesting();

    assertTrue(areMatchesShowing());
    const matchEls =
        realbox.$.matches.shadowRoot!.querySelectorAll('ntp-realbox-match');
    assertEquals(2, matchEls.length);
    // First match is selected.
    assertTrue(matchEls[0]!.classList.contains(Classes.SELECTED));

    // First match does not support deletion.
    const deleteEvent = new KeyboardEvent('keydown', {
      bubbles: true,
      cancelable: true,
      composed: true,  // So it propagates across shadow DOM boundary.
      key: 'Delete',
    });
    realbox.$.input.dispatchEvent(deleteEvent);
    assertFalse(deleteEvent.defaultPrevented);
    assertEquals(0, testProxy.handler.getCallCount('deleteAutocompleteMatch'));

    const arrowDownEvent = new KeyboardEvent('keydown', {
      bubbles: true,
      cancelable: true,
      composed: true,  // So it propagates across shadow DOM boundary.
      key: 'ArrowDown',
    });
    realbox.$.input.dispatchEvent(arrowDownEvent);
    assertTrue(arrowDownEvent.defaultPrevented);

    // Second match is selected.
    assertTrue(matchEls[1]!.classList.contains(Classes.SELECTED));

    // Unmodified 'Delete' key does not delete matches.
    realbox.$.input.dispatchEvent(deleteEvent);
    assertFalse(deleteEvent.defaultPrevented);
    assertEquals(0, testProxy.handler.getCallCount('deleteAutocompleteMatch'));

    const shiftDeleteEvent = new KeyboardEvent('keydown', {
      bubbles: true,
      cancelable: true,
      composed: true,  // So it propagates across shadow DOM boundary.
      key: 'Delete',
      shiftKey: true,
    });
    realbox.$.input.dispatchEvent(shiftDeleteEvent);
    assertTrue(shiftDeleteEvent.defaultPrevented);
    await testProxy.handler.whenCalled('deleteAutocompleteMatch')
        .then((args) => {
          assertEquals(1, args.line);
        });
    assertEquals(1, testProxy.handler.getCallCount('deleteAutocompleteMatch'));
  });

  test('Can remove match using the remove button', async () => {
    realbox.$.input.value = 'hello';
    realbox.$.input.dispatchEvent(new InputEvent('input'));

    const matches =
        [createSearchMatch(), createUrlMatch({supportsDeletion: true})];
    testProxy.callbackRouterRemote.autocompleteResultChanged({
      input: mojoString16(realbox.$.input.value.trimLeft()),
      matches,
      suggestionGroupsMap: {},
    });
    await testProxy.callbackRouterRemote.$.flushForTesting();

    assertTrue(areMatchesShowing());
    const matchEls =
        realbox.$.matches.shadowRoot!.querySelectorAll('ntp-realbox-match');
    assertEquals(2, matchEls.length);

    // By pressing 'Enter' on the button.
    const enter = new KeyboardEvent('keydown', {
      bubbles: true,
      cancelable: true,
      composed: true,  // So it propagates across shadow DOM boundary.
      key: 'Enter',
    });
    matchEls[1]!.$.remove.dispatchEvent(enter);
    assertTrue(enter.defaultPrevented);
    await testProxy.handler.whenCalled('deleteAutocompleteMatch')
        .then((args) => {
          assertEquals(1, args.line);
        });
    assertEquals(1, testProxy.handler.getCallCount('deleteAutocompleteMatch'));

    testProxy.handler.reset();

    // By clicking the button.
    matchEls[1]!.$.remove.click();
    await testProxy.handler.whenCalled('deleteAutocompleteMatch')
        .then((args) => {
          assertEquals(1, args.line);
        });
    assertEquals(1, testProxy.handler.getCallCount('deleteAutocompleteMatch'));
  });

  test('Selection is restored after selected match is removed', async () => {
    realbox.$.input.value = 'hello';
    realbox.$.input.dispatchEvent(new InputEvent('input'));

    let matches = [
      createSearchMatch({
        supportsDeletion: true,
      }),
    ];
    testProxy.callbackRouterRemote.autocompleteResultChanged({
      input: mojoString16(realbox.$.input.value.trimLeft()),
      matches,
      suggestionGroupsMap: {},
    });
    await testProxy.callbackRouterRemote.$.flushForTesting();

    assertTrue(areMatchesShowing());
    let matchEls =
        realbox.$.matches.shadowRoot!.querySelectorAll('ntp-realbox-match');
    assertEquals(1, matchEls.length);

    // First match is not selected.
    assertFalse(matchEls[0]!.classList.contains(Classes.SELECTED));

    // Remove the first match.
    matchEls[0]!.$.remove.click();
    await testProxy.handler.whenCalled('deleteAutocompleteMatch')
        .then((args) => {
          assertEquals(0, args.line);
        });
    assertEquals(1, testProxy.handler.getCallCount('deleteAutocompleteMatch'));

    testProxy.handler.reset();

    matches = [createUrlMatch({supportsDeletion: true})];
    testProxy.callbackRouterRemote.autocompleteResultChanged({
      input: mojoString16('hello'),
      matches,
      suggestionGroupsMap: {},
    });
    await testProxy.callbackRouterRemote.$.flushForTesting();

    assertTrue(areMatchesShowing());
    matchEls =
        realbox.$.matches.shadowRoot!.querySelectorAll('ntp-realbox-match');
    assertEquals(1, matchEls.length);

    // First match is not selected.
    assertFalse(matchEls[0]!.classList.contains(Classes.SELECTED));

    const arrowDownEvent = new KeyboardEvent('keydown', {
      bubbles: true,
      cancelable: true,
      composed: true,  // So it propagates across shadow DOM boundary.
      key: 'ArrowDown',
    });
    realbox.$.input.dispatchEvent(arrowDownEvent);
    assertTrue(arrowDownEvent.defaultPrevented);

    // First match is selected.
    assertTrue(matchEls[0]!.classList.contains(Classes.SELECTED));
    assertEquals('https://helloworld.com', realbox.$.input.value);

    // Remove the first match.
    const shiftDeleteEvent = new KeyboardEvent('keydown', {
      bubbles: true,
      cancelable: true,
      composed: true,  // So it propagates across shadow DOM boundary.
      key: 'Delete',
      shiftKey: true,
    });
    realbox.$.input.dispatchEvent(shiftDeleteEvent);
    assertTrue(shiftDeleteEvent.defaultPrevented);
    await testProxy.handler.whenCalled('deleteAutocompleteMatch')
        .then((args) => {
          assertEquals(0, args.line);
        });
    assertEquals(1, testProxy.handler.getCallCount('deleteAutocompleteMatch'));

    matches = [createSearchMatch()];
    testProxy.callbackRouterRemote.autocompleteResultChanged({
      input: mojoString16('hello'),
      matches,
      suggestionGroupsMap: {},
    });
    await testProxy.callbackRouterRemote.$.flushForTesting();

    assertTrue(areMatchesShowing());
    matchEls =
        realbox.$.matches.shadowRoot!.querySelectorAll('ntp-realbox-match');
    assertEquals(1, matchEls.length);

    // First match is selected.
    assertTrue(matchEls[0]!.classList.contains(Classes.SELECTED));
    assertEquals('hello world', realbox.$.input.value);
  });

  //============================================================================
  // Test Selection
  //============================================================================

  test('pressing Escape selects the first match / hides matches', async () => {
    realbox.$.input.value = 'hello';
    realbox.$.input.dispatchEvent(new InputEvent('input'));

    const matches = [createSearchMatch(), createUrlMatch()];
    testProxy.callbackRouterRemote.autocompleteResultChanged({
      input: mojoString16(realbox.$.input.value.trimLeft()),
      matches,
      suggestionGroupsMap: {},
    });
    await testProxy.callbackRouterRemote.$.flushForTesting();

    assertTrue(areMatchesShowing());
    let matchEls =
        realbox.$.matches.shadowRoot!.querySelectorAll('ntp-realbox-match');
    assertEquals(2, matchEls.length);

    // Select the second match.
    matchEls[1]!.focus();
    matchEls[1]!.dispatchEvent(new Event('focusin', {
      bubbles: true,
      cancelable: true,
      composed: true,  // So it propagates across shadow DOM boundary.
    }));
    assertTrue(matchEls[1]!.classList.contains(Classes.SELECTED));
    assertEquals('https://helloworld.com', realbox.$.input.value);
    assertEquals(matchEls[1], realbox.$.matches.shadowRoot!.activeElement);

    let escapeEvent = new KeyboardEvent('keydown', {
      bubbles: true,
      cancelable: true,
      composed: true,  // So it propagates across shadow DOM boundary.
      key: 'Escape',
    });
    realbox.$.input.dispatchEvent(escapeEvent);
    assertTrue(escapeEvent.defaultPrevented);

    // First match gets selected and also gets the focus.
    assertTrue(matchEls[0]!.classList.contains(Classes.SELECTED));
    assertEquals('hello world', realbox.$.input.value);
    assertEquals(matchEls[0], realbox.$.matches.shadowRoot!.activeElement);

    escapeEvent = new KeyboardEvent('keydown', {
      bubbles: true,
      cancelable: true,
      composed: true,  // So it propagates across shadow DOM boundary.
      key: 'Escape',
    });
    realbox.$.input.dispatchEvent(escapeEvent);
    assertTrue(escapeEvent.defaultPrevented);

    // Matches are hidden.
    assertFalse(areMatchesShowing());
    // Force a synchronous render.
    realbox.$.matches.$.groups.render();
    // Matches are cleared.
    matchEls =
        realbox.$.matches.shadowRoot!.querySelectorAll('ntp-realbox-match');
    assertEquals(0, matchEls.length);
    // Input is cleared.
    assertEquals('', realbox.$.input.value);

    // Show zero-prefix matches.
    realbox.$.input.dispatchEvent(new MouseEvent('mousedown', {button: 0}));
    testProxy.callbackRouterRemote.autocompleteResultChanged({
      input: mojoString16(''),
      matches,
      suggestionGroupsMap: {},
    });
    await testProxy.callbackRouterRemote.$.flushForTesting();

    assertTrue(areMatchesShowing());
    matchEls =
        realbox.$.matches.shadowRoot!.querySelectorAll('ntp-realbox-match');
    assertEquals(2, matchEls.length);

    // Pressing 'Escape' when no matches are selected closes the dropdown.
    escapeEvent = new KeyboardEvent('keydown', {
      bubbles: true,
      cancelable: true,
      composed: true,  // So it propagates across shadow DOM boundary.
      key: 'Escape',
    });
    realbox.$.input.dispatchEvent(escapeEvent);
    assertTrue(escapeEvent.defaultPrevented);

    // Matches are hidden.
    assertFalse(areMatchesShowing());
    // Force a synchronous render.
    realbox.$.matches.$.groups.render();
    // Matches are cleared.
    matchEls =
        realbox.$.matches.shadowRoot!.querySelectorAll('ntp-realbox-match');
    assertEquals(0, matchEls.length);
  });

  test('arrow up/down moves selection / focus', async () => {
    realbox.$.input.focus();
    realbox.$.input.value = 'hello';
    realbox.$.input.dispatchEvent(new InputEvent('input'));

    const matches = [createSearchMatch(), createUrlMatch()];
    testProxy.callbackRouterRemote.autocompleteResultChanged({
      input: mojoString16(realbox.$.input.value.trimLeft()),
      matches,
      suggestionGroupsMap: {},
    });
    await testProxy.callbackRouterRemote.$.flushForTesting();

    assertTrue(areMatchesShowing());
    const matchEls =
        realbox.$.matches.shadowRoot!.querySelectorAll('ntp-realbox-match');
    assertEquals(2, matchEls.length);

    let arrowDownEvent = new KeyboardEvent('keydown', {
      bubbles: true,
      cancelable: true,
      composed: true,  // So it propagates across shadow DOM boundary.
      key: 'ArrowDown',
    });
    realbox.$.input.dispatchEvent(arrowDownEvent);
    assertTrue(arrowDownEvent.defaultPrevented);

    // First match is selected but does not get focus while focus is in the
    // input.
    assertTrue(matchEls[0]!.classList.contains(Classes.SELECTED));
    assertEquals('hello world', realbox.$.input.value);
    assertEquals(realbox.$.input, realbox.shadowRoot!.activeElement);

    // If text is being composed with an IME composition selection is prevented.
    arrowDownEvent = new KeyboardEvent('keydown', {
      bubbles: true,
      cancelable: true,
      composed: true,  // So it propagates across shadow DOM boundary.
      isComposing: true,
      key: 'ArrowDown',
    });
    realbox.$.input.dispatchEvent(arrowDownEvent);
    assertFalse(arrowDownEvent.defaultPrevented);

    // First match remains selected and does not get focus while focus is in the
    // input.
    assertTrue(matchEls[0]!.classList.contains(Classes.SELECTED));
    assertEquals('hello world', realbox.$.input.value);
    assertEquals(realbox.$.input, realbox.shadowRoot!.activeElement);

    arrowDownEvent = new KeyboardEvent('keydown', {
      bubbles: true,
      cancelable: true,
      composed: true,  // So it propagates across shadow DOM boundary.
      key: 'ArrowDown',
    });
    realbox.$.input.dispatchEvent(arrowDownEvent);
    assertTrue(arrowDownEvent.defaultPrevented);

    // Second match gets selected but does not get focus while focus is in the
    // input.
    assertTrue(matchEls[1]!.classList.contains(Classes.SELECTED));
    assertEquals('https://helloworld.com', realbox.$.input.value);
    assertEquals(realbox.$.input, realbox.shadowRoot!.activeElement);

    // Move the focus to the second match.
    matchEls[1]!.focus();
    matchEls[1]!.dispatchEvent(new Event('focusin', {
      bubbles: true,
      cancelable: true,
      composed: true,  // So it propagates across shadow DOM boundary.
    }));

    // Second match is selected and has focus.
    assertTrue(matchEls[1]!.classList.contains(Classes.SELECTED));
    assertEquals('https://helloworld.com', realbox.$.input.value);
    assertEquals(matchEls[1], realbox.$.matches.shadowRoot!.activeElement);

    const arrowUpEvent = new KeyboardEvent('keydown', {
      bubbles: true,
      cancelable: true,
      composed: true,  // So it propagates across shadow DOM boundary.
      key: 'ArrowUp',
    });
    matchEls[1]!.dispatchEvent(arrowUpEvent);
    assertTrue(arrowUpEvent.defaultPrevented);

    // First match gets selected and gets focus while focus is in the matches.
    assertTrue(matchEls[0]!.classList.contains(Classes.SELECTED));
    assertEquals('hello world', realbox.$.input.value);
    assertEquals(matchEls[0], realbox.$.matches.shadowRoot!.activeElement);
  });

  test('focus indicator', async () => {
    realbox.$.input.focus();
    realbox.$.input.value = 'clear browsing history';
    realbox.$.input.dispatchEvent(new InputEvent('input'));

    const matches = [createSearchMatch({
      action: {
        a11yLabel: mojoString16(''),
        hint: mojoString16('Clear Browsing History'),
        suggestionContents: mojoString16(''),
        iconUrl: 'chrome://theme/current-channel-logo',
      },
      fillIntoEdit: mojoString16('clear browsing history'),
      supportsDeletion: true,
    })];
    testProxy.callbackRouterRemote.autocompleteResultChanged({
      input: mojoString16(realbox.$.input.value.trimLeft()),
      matches,
      suggestionGroupsMap: {},
    });
    await testProxy.callbackRouterRemote.$.flushForTesting();

    assertTrue(areMatchesShowing());
    const matchEls =
        realbox.$.matches.shadowRoot!.querySelectorAll('ntp-realbox-match');

    const focusIndicator = matchEls[0]!.$['focus-indicator'];

    // Select the first match
    const arrowDownEvent = new KeyboardEvent('keydown', {
      bubbles: true,
      cancelable: true,
      composed: true,  // So it propagates across shadow DOM boundary.
      key: 'ArrowDown',
    });
    realbox.$.input.dispatchEvent(arrowDownEvent);
    assertTrue(arrowDownEvent.defaultPrevented);

    assertTrue(matchEls[0]!.classList.contains(Classes.SELECTED));
    assertEquals('clear browsing history', realbox.$.input.value);
    assertEquals(window.getComputedStyle(focusIndicator).display, 'block');

    // Give focus to the action button
    const action = $$<HTMLElement>(matchEls[0]!, '#action')!;
    action.focus();

    assertTrue(matchEls[0]!.classList.contains(Classes.SELECTED));
    assertEquals(action, matchEls[0]!.shadowRoot!.activeElement);
    assertEquals(window.getComputedStyle(focusIndicator).display, 'none');

    // Give focus to remove button
    const removeButton = matchEls[0]!.$.remove;
    removeButton.focus();

    assertTrue(matchEls[0]!.classList.contains(Classes.SELECTED));
    assertEquals(removeButton, matchEls[0]!.shadowRoot!.activeElement);
    assertEquals(window.getComputedStyle(focusIndicator).display, 'none');
  });

  //============================================================================
  // Test Metrics
  //============================================================================

  test('responsiveness metric is being recorded', async () => {
    realbox.$.input.value = 'he';
    realbox.$.input.dispatchEvent(new InputEvent('input'));

    // The responsiveness metric is not recorded until the results are painted.
    assertEquals(
        0, testProxy.handler.getCallCount('logCharTypedToRepaintLatency'));

    let matches = [createSearchMatch()];
    testProxy.callbackRouterRemote.autocompleteResultChanged({
      input: mojoString16(realbox.$.input.value.trimLeft()),
      matches,
      suggestionGroupsMap: {},
    });
    await testProxy.callbackRouterRemote.$.flushForTesting();
    // The responsiveness metric is recorded once the results are painted.
    await testProxy.handler.whenCalled('logCharTypedToRepaintLatency')
        .then((args) => {
          assertTrue(args.timeDelta.microseconds > 0);
        });
    assertEquals(
        1, testProxy.handler.getCallCount('logCharTypedToRepaintLatency'));

    testProxy.handler.reset();

    // Delete the last character.
    realbox.$.input.value = 'h';
    realbox.$.input.dispatchEvent(new InputEvent('input'));

    matches = [createSearchMatch({
      allowedToBeDefaultMatch: true,
      inlineAutocompletion: mojoString16('ello'),
    })];
    testProxy.callbackRouterRemote.autocompleteResultChanged({
      input: mojoString16(realbox.$.input.value.trimLeft()),
      matches,
      suggestionGroupsMap: {},
    });
    await testProxy.callbackRouterRemote.$.flushForTesting();
    // The responsiveness metric is not recorded when characters are deleted.
    assertEquals(
        0, testProxy.handler.getCallCount('logCharTypedToRepaintLatency'));

    testProxy.handler.reset();

    assertEquals('hello', realbox.$.input.value);
    const start = realbox.$.input.selectionStart!;
    const end = realbox.$.input.selectionEnd!;
    assertEquals('ello', realbox.$.input.value.substring(start, end));

    // Type the next character of the inline autocompletion.
    const keyEvent = new KeyboardEvent('keydown', {
      bubbles: true,
      cancelable: true,
      composed: true,  // So it propagates across shadow DOM boundary.
      key: 'e',
    });
    realbox.$.input.dispatchEvent(keyEvent);
    assertTrue(keyEvent.defaultPrevented);

    matches = [createSearchMatch({
      allowedToBeDefaultMatch: true,
      inlineAutocompletion: mojoString16('llo'),
    })];
    testProxy.callbackRouterRemote.autocompleteResultChanged({
      input: mojoString16('he'),
      matches,
      suggestionGroupsMap: {},
    });
    await testProxy.callbackRouterRemote.$.flushForTesting();
    // The responsiveness metric is recorded when the default match has
    // inline autocompletion.
    await testProxy.handler.whenCalled('logCharTypedToRepaintLatency')
        .then((args) => {
          assertTrue(args.timeDelta.microseconds > 0);
        });
    assertEquals(
        1, testProxy.handler.getCallCount('logCharTypedToRepaintLatency'));
  });

  //============================================================================
  // Test favicons / entity images
  //============================================================================

  test(
      'match and realbox icons are updated when favicon becomes available',
      async () => {
        function assertBackgroundImageDataUrl(
            iconElement: RealboxIconElement, dataUrl: string) {
          assertStyle(
              iconElement.$.icon, 'background-image', `url("${dataUrl}")`);
          assertStyle(iconElement.$.icon, '-webkit-mask-image', 'none');
        }
        const faviconData = 'data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAC=';

        realbox.$.input.value = 'hello';
        realbox.$.input.dispatchEvent(new InputEvent('input'));

        const matches = [
          createSearchMatch({iconUrl: 'clock.svg'}),
          createUrlMatch({iconUrl: 'page.svg'}),
        ];
        testProxy.callbackRouterRemote.autocompleteResultChanged({
          input: mojoString16(realbox.$.input.value.trimLeft()),
          matches,
          suggestionGroupsMap: {},
        });
        await testProxy.callbackRouterRemote.$.flushForTesting();

        assertTrue(areMatchesShowing());
        const matchEls =
            realbox.$.matches.shadowRoot!.querySelectorAll('ntp-realbox-match');
        assertEquals(2, matchEls.length);
        assertIconMaskImageUrl(matchEls[0]!.$.icon, 'clock.svg');
        assertIconMaskImageUrl(matchEls[1]!.$.icon, 'page.svg');
        assertIconMaskImageUrl(realbox.$.icon, 'search.svg');  // Default icon.

        // Select the first match.
        let arrowDownEvent = new KeyboardEvent('keydown', {
          bubbles: true,
          cancelable: true,
          composed: true,  // So it propagates across shadow DOM boundary.
          key: 'ArrowDown',
        });
        realbox.$.input.dispatchEvent(arrowDownEvent);
        assertTrue(arrowDownEvent.defaultPrevented);

        // First match is selected.
        assertTrue(matchEls[0]!.classList.contains(Classes.SELECTED));
        // Input is updated.
        assertEquals('hello world', realbox.$.input.value);
        // Realbox icon is updated.
        assertIconMaskImageUrl(realbox.$.icon, 'clock.svg');

        // Select the second match.
        arrowDownEvent = new KeyboardEvent('keydown', {
          bubbles: true,
          cancelable: true,
          composed: true,  // So it propagates across shadow DOM boundary.
          key: 'ArrowDown',
        });
        realbox.$.input.dispatchEvent(arrowDownEvent);
        assertTrue(arrowDownEvent.defaultPrevented);

        // Second match is selected.
        assertTrue(matchEls[1]!.classList.contains(Classes.SELECTED));
        // Input is updated.
        assertEquals('https://helloworld.com', realbox.$.input.value);
        // Realbox icon is updated.
        assertIconMaskImageUrl(realbox.$.icon, 'page.svg');

        // URL of the loaded favicon must match destination URL of the match.
        testProxy.callbackRouterRemote.autocompleteMatchImageAvailable(
            1, {url: 'http://example.com/'}, faviconData);
        await testProxy.callbackRouterRemote.$.flushForTesting();
        assertIconMaskImageUrl(matchEls[1]!.$.icon, 'page.svg');
        assertIconMaskImageUrl(realbox.$.icon, 'page.svg');

        // Index of the loaded favicon must match index of the match.
        testProxy.callbackRouterRemote.autocompleteMatchImageAvailable(
            0, {url: 'https://helloworld.com/'}, faviconData);
        await testProxy.callbackRouterRemote.$.flushForTesting();
        assertIconMaskImageUrl(matchEls[1]!.$.icon, 'page.svg');
        assertIconMaskImageUrl(realbox.$.icon, 'page.svg');

        // Once the favicon successfully loads it replaces the match icon as
        // well as the realbox icon if the match is selected.
        testProxy.callbackRouterRemote.autocompleteMatchImageAvailable(
            1, {url: 'https://helloworld.com/'}, faviconData);
        await testProxy.callbackRouterRemote.$.flushForTesting();
        assertBackgroundImageDataUrl(matchEls[1]!.$.icon, faviconData);
        assertBackgroundImageDataUrl(realbox.$.icon, faviconData);

        // Select the first match by pressing 'Escape'.
        const escapeEvent = new KeyboardEvent('keydown', {
          bubbles: true,
          cancelable: true,
          composed: true,  // So it propagates across shadow DOM boundary.
          key: 'Escape',
        });
        realbox.$.input.dispatchEvent(escapeEvent);
        assertTrue(escapeEvent.defaultPrevented);

        // First match is selected.
        assertTrue(matchEls[0]!.classList.contains(Classes.SELECTED));
        // Input is updated.
        assertEquals('hello world', realbox.$.input.value);
        // Realbox icon is updated.
        assertIconMaskImageUrl(realbox.$.icon, 'clock.svg');
      });

  test(
      'realbox icons is updated when url match is cut from realbox',
      async () => {
        realbox.$.input.value = 'www.test.com';
        realbox.$.input.dispatchEvent(new InputEvent('input'));

        const matches = [createUrlMatch(
            {allowedToBeDefaultMatch: true, iconUrl: 'page.svg'})];

        testProxy.callbackRouterRemote.autocompleteResultChanged({
          input: mojoString16(realbox.$.input.value.trimLeft()),
          matches,
          suggestionGroupsMap: {},
        });
        await testProxy.callbackRouterRemote.$.flushForTesting();

        assertIconMaskImageUrl(realbox.$.icon, 'page.svg');
        // Select the entire input.
        realbox.$.input.setSelectionRange(0, realbox.$.input.value.length);

        const cutEvent = createClipboardEvent('cut');
        realbox.$.input.dispatchEvent(cutEvent);
        assertTrue(cutEvent.defaultPrevented);

        assertIconMaskImageUrl(realbox.$.icon, 'search.svg');
      });

  test(
      'match icons are updated when entity images become available',
      async () => {
        const imageData = 'data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAC=';

        realbox.$.input.value = 'hello';
        realbox.$.input.dispatchEvent(new InputEvent('input'));

        const matches = [
          createUrlMatch({iconUrl: 'page.svg'}),
          createSearchMatch({
            iconUrl: 'clock.svg',
            imageUrl: 'https://gstatic.com/',
            imageDominantColor: '#757575',
            isRichSuggestion: true,
          }),
        ];
        testProxy.callbackRouterRemote.autocompleteResultChanged({
          input: mojoString16(realbox.$.input.value.trimLeft()),
          matches,
          suggestionGroupsMap: {},
        });
        await testProxy.callbackRouterRemote.$.flushForTesting();

        assertTrue(areMatchesShowing());
        const matchEls =
            realbox.$.matches.shadowRoot!.querySelectorAll('ntp-realbox-match');
        assertEquals(2, matchEls.length);
        assertIconMaskImageUrl(matchEls[0]!.$.icon, 'page.svg');
        assertIconMaskImageUrl(matchEls[1]!.$.icon, 'clock.svg');
        assertIconMaskImageUrl(realbox.$.icon, 'search.svg');  // Default icon.

        // Select the first match.
        let arrowDownEvent = new KeyboardEvent('keydown', {
          bubbles: true,
          cancelable: true,
          composed: true,  // So it propagates across shadow DOM boundary.
          key: 'ArrowDown',
        });
        realbox.$.input.dispatchEvent(arrowDownEvent);
        assertTrue(arrowDownEvent.defaultPrevented);

        // First match is selected.
        assertTrue(matchEls[0]!.classList.contains(Classes.SELECTED));
        // Input is updated.
        assertEquals('https://helloworld.com', realbox.$.input.value);
        // Realbox icon is updated.
        assertIconMaskImageUrl(realbox.$.icon, 'page.svg');

        // Select the second match.
        arrowDownEvent = new KeyboardEvent('keydown', {
          bubbles: true,
          cancelable: true,
          composed: true,  // So it propagates across shadow DOM boundary.
          key: 'ArrowDown',
        });
        realbox.$.input.dispatchEvent(arrowDownEvent);
        assertTrue(arrowDownEvent.defaultPrevented);

        // Second match is selected.
        assertTrue(matchEls[1]!.classList.contains(Classes.SELECTED));
        // Input is updated.
        assertEquals('hello world', realbox.$.input.value);
        // Second match shows a placeholder color until the image loads.
        const containerEl = matchEls[1]!.$.icon.$.container;
        assertStyle(
            containerEl, 'background-color', 'rgba(117, 117, 117, 0.25)');
        assertIconMaskImageUrl(realbox.$.icon, 'search.svg');  // Default icon.

        // URL of the loaded image must match image URL of the match.
        testProxy.callbackRouterRemote.autocompleteMatchImageAvailable(
            1, {url: 'http://example.com/'}, imageData);
        await testProxy.callbackRouterRemote.$.flushForTesting();
        assertStyle(
            containerEl, 'background-color', 'rgba(117, 117, 117, 0.25)');
        assertIconMaskImageUrl(realbox.$.icon, 'search.svg');  // Default icon.

        // Index of the loaded image must match index of the match.
        testProxy.callbackRouterRemote.autocompleteMatchImageAvailable(
            0, {url: 'https://gstatic.com/'}, imageData);
        await testProxy.callbackRouterRemote.$.flushForTesting();
        assertStyle(
            containerEl, 'background-color', 'rgba(117, 117, 117, 0.25)');
        assertIconMaskImageUrl(realbox.$.icon, 'search.svg');  // Default icon.

        // Once the image successfully loads it replaces the match icon.
        testProxy.callbackRouterRemote.autocompleteMatchImageAvailable(
            1, {url: 'https://gstatic.com/'}, imageData);
        await testProxy.callbackRouterRemote.$.flushForTesting();
        assertEquals(
            matchEls[1]!.$.icon.$.image.getAttribute('src'), imageData);
        assertStyle(containerEl, 'background-color', 'rgba(0, 0, 0, 0)');
        // Realbox icon is not updated as the input does not feature images.
        assertIconMaskImageUrl(realbox.$.icon, 'search.svg');  // Default icon.
        assertTrue(window.getComputedStyle(realbox.$.icon).display !== 'none');

        // Select the first match by pressing 'Escape'.
        const escapeEvent = new KeyboardEvent('keydown', {
          bubbles: true,
          cancelable: true,
          composed: true,  // So it propagates across shadow DOM boundary.
          key: 'Escape',
        });
        realbox.$.input.dispatchEvent(escapeEvent);
        assertTrue(escapeEvent.defaultPrevented);

        // First match is selected.
        assertTrue(matchEls[0]!.classList.contains(Classes.SELECTED));
        // Input is updated.
        assertEquals('https://helloworld.com', realbox.$.input.value);
        // Realbox icon is updated.
        assertIconMaskImageUrl(realbox.$.icon, 'page.svg');
      });

  //============================================================================
  // Test suggestion groups
  //============================================================================

  test('matches in a suggestion group can be made hidden/visible', async () => {
    realbox.$.input.value = 'hello';
    realbox.$.input.dispatchEvent(new InputEvent('input'));

    const matches =
        [createSearchMatch(), createUrlMatch({suggestionGroupId: 100})];
    const suggestionGroupsMap = {
      100: {
        header: mojoString16('Recommended for you'),
        hideGroupA11yLabel: mojoString16(''),
        showGroupA11yLabel: mojoString16(''),
        hidden: true,
      },
      101: {
        header: mojoString16('Not recommended for you'),
        hideGroupA11yLabel: mojoString16(''),
        showGroupA11yLabel: mojoString16(''),
        hidden: false,
      },
    };
    testProxy.callbackRouterRemote.autocompleteResultChanged({
      input: mojoString16(realbox.$.input.value.trimLeft()),
      matches,
      suggestionGroupsMap,
    });
    await testProxy.callbackRouterRemote.$.flushForTesting();

    // The first match is showing. The second match is initially hidden.
    assertTrue(areMatchesShowing());
    let matchEls =
        realbox.$.matches.shadowRoot!.querySelectorAll('ntp-realbox-match');
    assertEquals(1, matchEls.length);

    // The suggestion group header and the toggle button are visible.
    const headerEl =
        realbox.$.matches.shadowRoot!.querySelectorAll<HTMLElement>(
            '.header')[0]!;
    assertTrue(window.getComputedStyle(headerEl).display !== 'none');
    assertEquals('Recommended for you', headerEl.textContent!.trim());
    const toggleButtonEl =
        realbox.$.matches.shadowRoot!.querySelectorAll('cr-icon-button')[0]!;
    assertTrue(window.getComputedStyle(toggleButtonEl).display !== 'none');

    // Make the second match visible by pressing 'Space' on the toggle button.
    toggleButtonEl.dispatchEvent(new KeyboardEvent('keydown', {
      bubbles: true,
      cancelable: true,
      composed: true,  // So it propagates across shadow DOM boundary.
      key: ' ',
    }));
    toggleButtonEl.dispatchEvent(new KeyboardEvent('keyup', {
      bubbles: true,
      cancelable: true,
      composed: true,  // So it propagates across shadow DOM boundary.
      key: ' ',
    }));

    await testProxy.handler.whenCalled('toggleSuggestionGroupIdVisibility')
        .then((args) => {
          assertEquals(100, args.suggestionGroupId);
        });
    assertEquals(
        1, testProxy.handler.getCallCount('toggleSuggestionGroupIdVisibility'));

    testProxy.handler.reset();

    // Second match is visible.
    matchEls =
        realbox.$.matches.shadowRoot!.querySelectorAll('ntp-realbox-match');
    assertEquals(2, matchEls.length);

    // Hide the second match by clicking the toggle button.
    toggleButtonEl.click();

    await testProxy.handler.whenCalled('toggleSuggestionGroupIdVisibility')
        .then((args) => {
          assertEquals(100, args.suggestionGroupId);
        });
    assertEquals(
        1, testProxy.handler.getCallCount('toggleSuggestionGroupIdVisibility'));

    // Second match is hidden.
    matchEls =
        realbox.$.matches.shadowRoot!.querySelectorAll('ntp-realbox-match');
    assertEquals(1, matchEls.length);

    testProxy.handler.reset();

    // Show the second match by clicking the header.
    headerEl.click();
    await testProxy.handler.whenCalled('toggleSuggestionGroupIdVisibility')
        .then((args) => {
          assertEquals(100, args.suggestionGroupId);
        });
    assertEquals(
        1, testProxy.handler.getCallCount('toggleSuggestionGroupIdVisibility'));
    // Second match is visible again.
    matchEls =
        realbox.$.matches.shadowRoot!.querySelectorAll('ntp-realbox-match');
    assertEquals(2, matchEls.length);
  });

  test(
      'focusing suggestion group header resets selection and input text',
      async () => {
        realbox.$.input.value = '';
        realbox.$.input.dispatchEvent(new MouseEvent('mousedown', {button: 0}));

        const matches =
            [createSearchMatch(), createUrlMatch({suggestionGroupId: 100})];
        const suggestionGroupsMap = {
          100: {
            header: mojoString16('Recommended for you'),
            hideGroupA11yLabel: mojoString16(''),
            showGroupA11yLabel: mojoString16(''),
            hidden: false,
          },
        };
        testProxy.callbackRouterRemote.autocompleteResultChanged({
          input: mojoString16(realbox.$.input.value.trimLeft()),
          matches,
          suggestionGroupsMap,
        });
        await testProxy.callbackRouterRemote.$.flushForTesting();

        assertTrue(areMatchesShowing());
        const matchEls =
            realbox.$.matches.shadowRoot!.querySelectorAll('ntp-realbox-match');
        assertEquals(2, matchEls.length);

        // Select the first match.
        matchEls[0]!.dispatchEvent(new Event('focusin', {
          bubbles: true,
          cancelable: true,
          composed: true,  // So it propagates across shadow DOM boundary.
        }));

        // First match is selected.
        assertTrue(matchEls[0]!.classList.contains(Classes.SELECTED));
        // Input is updated.
        assertEquals('hello world', realbox.$.input.value);

        // Focus the suggestion group header.
        const headerEl =
            realbox.$.matches.shadowRoot!.querySelectorAll<HTMLElement>(
                '.header')[0]!;
        headerEl.dispatchEvent(new Event('focusin', {
          bubbles: true,
          cancelable: true,
          composed: true,  // So it propagates across shadow DOM boundary.
        }));

        // First match is no longer selected.
        assertFalse(matchEls[0]!.classList.contains(Classes.SELECTED));
        // Input is cleared.
        assertEquals('', realbox.$.input.value);
      });

  //============================================================================
  // Test calculator answer type
  //============================================================================

  test('match calculator answer type', async () => {
    const matches = [createCalculatorMatch({isRichSuggestion: true})];

    realbox.$.input.value = '2 + 3';
    realbox.$.input.dispatchEvent(new InputEvent('input'));

    testProxy.callbackRouterRemote.autocompleteResultChanged({
      input: mojoString16(realbox.$.input.value.trimLeft()),
      matches,
      suggestionGroupsMap: {},
    });
    await testProxy.callbackRouterRemote.$.flushForTesting();

    assertTrue(areMatchesShowing());
    const matchEls =
        realbox.$.matches.shadowRoot!.querySelectorAll('ntp-realbox-match');
    assertEquals(1, matchEls.length);

    verifyMatch(matches[0]!, matchEls[0]!);
    assertIconMaskImageUrl(matchEls[0]!.$.icon, 'calculator.svg');
    assertIconMaskImageUrl(realbox.$.icon, 'search.svg');

    // Separator is not displayed
    assertEquals(
        window.getComputedStyle(matchEls[0]!.$.separator).display, 'none');

    const arrowDownEvent = new KeyboardEvent('keydown', {
      bubbles: true,
      cancelable: true,
      composed: true,  // So it propagates across shadow DOM boundary.
      key: 'ArrowDown',
    });
    realbox.$.input.dispatchEvent(arrowDownEvent);
    assertTrue(arrowDownEvent.defaultPrevented);

    assertTrue(matchEls[0]!.classList.contains(Classes.SELECTED));
    assertEquals('5', realbox.$.input.value);

    assertIconMaskImageUrl(realbox.$.icon, 'search.svg');  // Default Icon
  });

  //============================================================================
  // Test suggestion answer
  //============================================================================

  test('Test Rich Suggestion Answer for Verbatim Question', async () => {
    realbox.$.input.value = 'When is Christmas Day';
    realbox.$.input.dispatchEvent(new InputEvent('input'));
    const matches = [createSearchMatch({
      answer: {
        firstLine: mojoString16('When is Christmas Day'),
        secondLine: mojoString16('Saturday, December 25, 2021'),
      },
      isRichSuggestion: true,
    })];
    testProxy.callbackRouterRemote.autocompleteResultChanged({
      input: mojoString16(realbox.$.input.value.trimLeft()),
      matches,
      suggestionGroupsMap: {},
    });
    await testProxy.callbackRouterRemote.$.flushForTesting();
    assertTrue(areMatchesShowing());
    const matchEls =
        realbox.$.matches.shadowRoot!.querySelectorAll('ntp-realbox-match');
    verifyMatch(matches[0]!, matchEls[0]!);

    // Separator is not displayed
    assertEquals(
        window.getComputedStyle(matchEls[0]!.$.separator).display, 'none');

    const arrowDownEvent = new KeyboardEvent('keydown', {
      bubbles: true,
      cancelable: true,
      composed: true,  // So it propagates across shadow DOM boundary.
      key: 'ArrowDown',
    });
    realbox.$.input.dispatchEvent(arrowDownEvent);
    assertTrue(arrowDownEvent.defaultPrevented);

    assertTrue(matchEls[0]!.classList.contains(Classes.SELECTED));

    assertIconMaskImageUrl(realbox.$.icon, 'search.svg');  // Default Icon
  });

  //============================================================================
  // Test pedals
  //============================================================================

  test('Test Actions for Verbatim Query', async () => {
    realbox.$.input.value = 'Clear Browsing History';
    realbox.$.input.dispatchEvent(new InputEvent('input'));
    const matches = [createSearchMatch({
      action: {
        a11yLabel: mojoString16(''),
        hint: mojoString16('Clear Browsing History'),
        suggestionContents: mojoString16(''),
        iconUrl: 'chrome://theme/current-channel-logo',
      },
    })];
    testProxy.callbackRouterRemote.autocompleteResultChanged({
      input: mojoString16(realbox.$.input.value.trimLeft()),
      matches,
      suggestionGroupsMap: {},
    });
    await testProxy.callbackRouterRemote.$.flushForTesting();
    assertTrue(areMatchesShowing());

    const matchEl = $$(realbox.$.matches, 'ntp-realbox-match')!;
    verifyMatch(matches[0]!, matchEl);

    const pedalEl = $$($$(matchEl, 'ntp-realbox-action')!, '.contents')!;

    assertEquals(
        pedalEl.querySelector<HTMLImageElement>('#action-icon')!.src,
        'chrome://theme/current-channel-logo');  // Default Pedal
                                                 // Icon

    const leftClick = new MouseEvent('click', {
      bubbles: true,
      button: 1,
      cancelable: true,
      composed: true,  // So it propagates across shadow DOM boundary.
      ctrlKey: true,
    });

    pedalEl.dispatchEvent(leftClick);
    assertTrue(leftClick.defaultPrevented);

    await testProxy.handler.whenCalled('executeAction').then((args) => {
      assertTrue(args.ctrlKey);
      assertEquals(0, args.line);
      assertEquals(args.mouseButton, 1);
      assertTrue(args.matchSelectionTimestamp['internalValue'] > 0);
    });
    assertEquals(1, testProxy.handler.getCallCount('executeAction'));
  });

  test('Test Actions for Autocomplete Query', async () => {
    realbox.$.input.value = 'Clear Bro';
    realbox.$.input.dispatchEvent(new InputEvent('input'));
    const matches = [
      createSearchMatch({contents: mojoString16('Clear Bro')}),
      createSearchMatch({
        action: {
          a11yLabel: mojoString16(''),
          hint: mojoString16('Clear Browsing History'),
          suggestionContents: mojoString16(''),
          iconUrl: 'chrome://theme/current-channel-logo',
        },
      }),
    ];
    testProxy.callbackRouterRemote.autocompleteResultChanged({
      input: mojoString16(realbox.$.input.value.trimLeft()),
      matches,
      suggestionGroupsMap: {},
    });
    await testProxy.callbackRouterRemote.$.flushForTesting();
    assertTrue(areMatchesShowing());

    const matchEls =
        realbox.$.matches.shadowRoot!.querySelectorAll('ntp-realbox-match');
    verifyMatch(matches[0]!, matchEls[0]!);
    verifyMatch(matches[1]!, matchEls[1]!);

    const pedalEl = $$($$(matchEls[1]!, 'ntp-realbox-action')!, '.contents')!;

    assertEquals(
        pedalEl.querySelector<HTMLImageElement>('#action-icon')!.src,
        'chrome://theme/current-channel-logo');  // Default Pedal
                                                 // Icon

    const leftClick = new MouseEvent('click', {
      bubbles: true,
      button: 0,
      cancelable: true,
      composed: true,  // So it propagates across shadow DOM boundary.
    });

    pedalEl.dispatchEvent(leftClick);
    assertTrue(leftClick.defaultPrevented);

    await testProxy.handler.whenCalled('executeAction').then((args) => {
      assertEquals(1, args.line);
      assertEquals(args.mouseButton, 0);
      assertTrue(args.matchSelectionTimestamp['internalValue'] > 0);
    });
    assertEquals(1, testProxy.handler.getCallCount('executeAction'));
  });
});
