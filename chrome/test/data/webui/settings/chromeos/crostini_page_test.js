// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @type {?SettingsCrostiniPageElement} */
let crostiniPage = null;

/** @type {?TestCrostiniBrowserProxy} */
let crostiniBrowserProxy = null;

function setCrostiniPrefs(enabled, optional = {}) {
  const {
    sharedPaths = {},
    sharedUsbDevices = [],
    forwardedPorts = [],
    crostiniMicSharingEnabled = false
  } = optional;
  crostiniPage.prefs = {
    crostini: {
      enabled: {value: enabled},
      port_forwarding: {ports: {value: forwardedPorts}},
    },
    guest_os: {
      paths_shared_to_vms: {value: sharedPaths},
    },
  };
  crostiniBrowserProxy.sharedUsbDevices = sharedUsbDevices;
  crostiniBrowserProxy.crostiniMicSharingEnabled = crostiniMicSharingEnabled;
  Polymer.dom.flush();
}

/**
 * Checks whether a given element is visible to the user.
 * @param {!Element} element
 * @returns {boolean}
 */
function isVisible(element) {
  return !!(element && element.getBoundingClientRect().width > 0);
}

suite('CrostiniPageTests', function() {
  setup(function() {
    crostiniBrowserProxy = new TestCrostiniBrowserProxy();
    settings.CrostiniBrowserProxyImpl.instance_ = crostiniBrowserProxy;
    PolymerTest.clearBody();
    crostiniPage = document.createElement('settings-crostini-page');
    document.body.appendChild(crostiniPage);
    testing.Test.disableAnimationsAndTransitions();
  });

  teardown(function() {
    console.log('Teardown has begun.');
    crostiniPage.remove();
    console.log('Teardown has ended.');
  });

  suite('MainPage', function() {
    setup(function() {
      setCrostiniPrefs(false);
    });

    test('Enable', function() {
      const button = crostiniPage.$$('#enable');
      assertTrue(!!button);
      assertFalse(!!crostiniPage.$$('.subpage-arrow'));
      assertFalse(button.disabled);

      button.click();
      Polymer.dom.flush();
      assertEquals(
          1, crostiniBrowserProxy.getCallCount('requestCrostiniInstallerView'));
      setCrostiniPrefs(true);

      assertTrue(!!crostiniPage.$$('.subpage-arrow'));
    });

    test('ButtonDisabledDuringInstall', async function() {
      const button = crostiniPage.$$('#enable');
      assertTrue(!!button);

      await test_util.flushTasks();
      assertFalse(button.disabled);
      cr.webUIListenerCallback('crostini-installer-status-changed', true);

      await test_util.flushTasks();
      assertTrue(button.disabled);
      cr.webUIListenerCallback('crostini-installer-status-changed', false);

      await test_util.flushTasks();
      assertFalse(button.disabled);
    });
  });

  suite('SubPageDetails', function() {
    /** @type {?SettingsCrostiniSubPageElement} */
    let subpage;

    setup(async function() {
      setCrostiniPrefs(true);
      loadTimeData.overrideValues({
        showCrostiniExportImport: true,
        showCrostiniContainerUpgrade: true,
        showCrostiniPortForwarding: true,
        showCrostiniMic: true,
        showCrostiniDiskResize: true,
      });

      const eventPromise = new Promise((resolve) => {
                             const v = cr.addWebUIListener(
                                 'crostini-installer-status-changed', () => {
                                   resolve(v);
                                 });
                           }).then((v) => {
        assertTrue(cr.removeWebUIListener(v));
      });

      settings.Router.getInstance().navigateTo(settings.routes.CROSTINI);
      crostiniPage.$$('#crostini').click();

      const pageLoadPromise = test_util.flushTasks().then(() => {
        subpage = crostiniPage.$$('settings-crostini-subpage');
        assertTrue(!!subpage);
      });

      await Promise.all([pageLoadPromise, eventPromise]);
    });

    suite('SubPageDefault', function() {
      test('Sanity', function() {
        assertTrue(!!subpage.$$('#crostini-shared-paths'));
        assertTrue(!!subpage.$$('#crostini-shared-usb-devices'));
        assertTrue(!!subpage.$$('#crostini-export-import'));
        assertTrue(!!subpage.$$('#remove'));
        assertTrue(!!subpage.$$('#container-upgrade'));
        assertTrue(!!subpage.$$('#crostini-port-forwarding'));
        assertTrue(!!subpage.$$('#crostini-mic-sharing-toggle'));
        assertTrue(!!subpage.$$('#crostini-disk-resize'));
      });

      test('SharedPaths', async function() {
        assertTrue(!!subpage.$$('#crostini-shared-paths'));
        subpage.$$('#crostini-shared-paths').click();

        await test_util.flushTasks();
        subpage = crostiniPage.$$('settings-crostini-shared-paths');
        assertTrue(!!subpage);
      });

      test('ContainerUpgrade', function() {
        assertTrue(!!subpage.$$('#container-upgrade cr-button'));
        subpage.$$('#container-upgrade cr-button').click();
        assertEquals(
            1,
            crostiniBrowserProxy.getCallCount(
                'requestCrostiniContainerUpgradeView'));
      });

      test('ContainerUpgradeButtonDisabledOnUpgradeDialog', async function() {
        const button = subpage.$$('#container-upgrade cr-button');
        assertTrue(!!button);

        await test_util.flushTasks();
        assertFalse(button.disabled);
        cr.webUIListenerCallback('crostini-upgrader-status-changed', true);

        await test_util.flushTasks();
        assertTrue(button.disabled);
        cr.webUIListenerCallback('crostini-upgrader-status-changed', false);

        await test_util.flushTasks();
        assertFalse(button.disabled);
      });

      test('ContainerUpgradeButtonDisabledOnInstall', async function() {
        const button = subpage.$$('#container-upgrade cr-button');
        assertTrue(!!button);

        await test_util.flushTasks();
        assertFalse(button.disabled);
        cr.webUIListenerCallback('crostini-installer-status-changed', true);

        await test_util.flushTasks();
        assertTrue(button.disabled);
        cr.webUIListenerCallback('crostini-installer-status-changed', false);

        await test_util.flushTasks();
        assertFalse(button.disabled);
      });

      test('Export', async function() {
        assertTrue(!!subpage.$$('#crostini-export-import'));
        subpage.$$('#crostini-export-import').click();

        await test_util.flushTasks();
        subpage = crostiniPage.$$('settings-crostini-export-import');
        assertTrue(!!subpage.$$('#export cr-button'));
        subpage.$$('#export cr-button').click();
        assertEquals(
            1, crostiniBrowserProxy.getCallCount('exportCrostiniContainer'));
      });

      test('Import', async function() {
        assertTrue(!!subpage.$$('#crostini-export-import'));
        subpage.$$('#crostini-export-import').click();

        await test_util.flushTasks();
        subpage = crostiniPage.$$('settings-crostini-export-import');
        subpage.$$('#import cr-button').click();

        await test_util.flushTasks();
        subpage = subpage.$$('settings-crostini-import-confirmation-dialog');
        subpage.$$('cr-dialog cr-button[id="continue"]').click();
        assertEquals(
            1, crostiniBrowserProxy.getCallCount('importCrostiniContainer'));
      });

      test('ExportImportButtonsGetDisabledOnOperationStatus', async function() {
        assertTrue(!!subpage.$$('#crostini-export-import'));
        subpage.$$('#crostini-export-import').click();

        await test_util.flushTasks();
        subpage = crostiniPage.$$('settings-crostini-export-import');
        assertFalse(subpage.$$('#export cr-button').disabled);
        assertFalse(subpage.$$('#import cr-button').disabled);
        cr.webUIListenerCallback(
            'crostini-export-import-operation-status-changed', true);

        await test_util.flushTasks();
        subpage = crostiniPage.$$('settings-crostini-export-import');
        assertTrue(subpage.$$('#export cr-button').disabled);
        assertTrue(subpage.$$('#import cr-button').disabled);
        cr.webUIListenerCallback(
            'crostini-export-import-operation-status-changed', false);

        await test_util.flushTasks();
        subpage = crostiniPage.$$('settings-crostini-export-import');
        assertFalse(subpage.$$('#export cr-button').disabled);
        assertFalse(subpage.$$('#import cr-button').disabled);
      });

      test(
          'ExportImportButtonsDisabledOnWhenInstallingCrostini',
          async function() {
            assertTrue(!!subpage.$$('#crostini-export-import'));
            subpage.$$('#crostini-export-import').click();

            await test_util.flushTasks();
            subpage = crostiniPage.$$('settings-crostini-export-import');
            assertFalse(subpage.$$('#export cr-button').disabled);
            assertFalse(subpage.$$('#import cr-button').disabled);
            cr.webUIListenerCallback('crostini-installer-status-changed', true);

            await test_util.flushTasks();
            subpage = crostiniPage.$$('settings-crostini-export-import');
            assertTrue(subpage.$$('#export cr-button').disabled);
            assertTrue(subpage.$$('#import cr-button').disabled);
            cr.webUIListenerCallback(
                'crostini-installer-status-changed', false);

            await test_util.flushTasks();
            subpage = crostiniPage.$$('settings-crostini-export-import');
            assertFalse(subpage.$$('#export cr-button').disabled);
            assertFalse(subpage.$$('#import cr-button').disabled);
          });

      test('ToggleCrostiniMicSharingCancel', async function() {
        // Crostini is assumed to be running when the page is loaded.
        assertTrue(!!subpage.$$('#crostini-mic-sharing-toggle'));
        assertFalse(!!subpage.$$('settings-crostini-mic-sharing-dialog'));

        setCrostiniPrefs(true, {crostiniMicSharingEnabled: true});
        cr.webUIListenerCallback(
            'crostini-mic-sharing-enabled-changed',
            crostiniBrowserProxy.crostiniMicSharingEnabled);
        assertTrue(subpage.$$('#crostini-mic-sharing-toggle').checked);

        subpage.$$('#crostini-mic-sharing-toggle').click();
        await test_util.flushTasks();
        assertTrue(!!subpage.$$('settings-crostini-mic-sharing-dialog'));
        const dialog = subpage.$$('settings-crostini-mic-sharing-dialog');
        const dialogClosedPromise = test_util.eventToPromise('close', dialog);
        dialog.$$('#cancel').click();
        await Promise.all([dialogClosedPromise, test_util.flushTasks()]);

        // Because the dialog was cancelled, the toggle should not have changed.
        assertFalse(!!subpage.$$('settings-crostini-mic-sharing-dialog'));
        assertTrue(subpage.$$('#crostini-mic-sharing-toggle').checked);
      });

      test('ToggleCrostiniMicSharingShutdown', async function() {
        // Crostini is assumed to be running when the page is loaded.
        assertTrue(!!subpage.$$('#crostini-mic-sharing-toggle'));
        assertFalse(!!subpage.$$('settings-crostini-mic-sharing-dialog'));

        setCrostiniPrefs(true, {crostiniMicSharingEnabled: false});
        cr.webUIListenerCallback(
            'crostini-mic-sharing-enabled-changed',
            crostiniBrowserProxy.crostiniMicSharingEnabled);
        assertFalse(subpage.$$('#crostini-mic-sharing-toggle').checked);

        subpage.$$('#crostini-mic-sharing-toggle').click();
        await test_util.flushTasks();
        assertTrue(!!subpage.$$('settings-crostini-mic-sharing-dialog'));
        const dialog = subpage.$$('settings-crostini-mic-sharing-dialog');
        const dialogClosedPromise = test_util.eventToPromise('close', dialog);
        dialog.$$('#shutdown').click();
        await Promise.all([dialogClosedPromise, test_util.flushTasks()]);
        assertEquals(1, crostiniBrowserProxy.getCallCount('shutdownCrostini'));
        assertEquals(
            1,
            crostiniBrowserProxy.getCallCount('setCrostiniMicSharingEnabled'));
        cr.webUIListenerCallback(
            'crostini-mic-sharing-enabled-changed',
            crostiniBrowserProxy.crostiniMicSharingEnabled);
        assertFalse(!!subpage.$$('settings-crostini-mic-sharing-dialog'));
        assertTrue(subpage.$$('#crostini-mic-sharing-toggle').checked);

        // Crostini is now shutdown, this means that it doesn't need to be
        // restarted in order for changes to take effect, therefore no dialog is
        // needed and the mic sharing settings can be changed immediately.
        subpage.$$('#crostini-mic-sharing-toggle').click();
        await test_util.flushTasks();
        cr.webUIListenerCallback(
            'crostini-mic-sharing-enabled-changed',
            crostiniBrowserProxy.crostiniMicSharingEnabled);
        assertFalse(!!subpage.$$('settings-crostini-mic-sharing-dialog'));
        assertFalse(subpage.$$('#crostini-mic-sharing-toggle').checked);
      });

      test('Remove', async function() {
        assertTrue(!!subpage.$$('#remove cr-button'));
        subpage.$$('#remove cr-button').click();
        assertEquals(
            1, crostiniBrowserProxy.getCallCount('requestRemoveCrostini'));
        setCrostiniPrefs(false);

        await test_util.eventToPromise('popstate', window);
        assertEquals(
            settings.Router.getInstance().getCurrentRoute(),
            settings.routes.CROSTINI);
        assertTrue(!!crostiniPage.$$('#enable'));
      });

      test('RemoveHidden', async function() {
        // Elements are not destroyed when a dom-if stops being shown, but we
        // can check if their rendered width is non-zero. This should be
        // resilient against most formatting changes, since we're not relying on
        // them having any exact size, or on Polymer using any particular means
        // of hiding elements.
        assertTrue(!!subpage.shadowRoot.querySelector('#remove').clientWidth);
        cr.webUIListenerCallback('crostini-installer-status-changed', true);

        await test_util.flushTasks();
        assertFalse(!!subpage.shadowRoot.querySelector('#remove').clientWidth);
        cr.webUIListenerCallback('crostini-installer-status-changed', false);

        await test_util.flushTasks();
        assertTrue(!!subpage.shadowRoot.querySelector('#remove').clientWidth);
      });

      test('HideOnDisable', async function() {
        assertEquals(
            settings.Router.getInstance().getCurrentRoute(),
            settings.routes.CROSTINI_DETAILS);
        setCrostiniPrefs(false);

        await test_util.eventToPromise('popstate', window);
        assertEquals(
            settings.Router.getInstance().getCurrentRoute(),
            settings.routes.CROSTINI);
      });

      test('DiskResizeOpensWhenClicked', async function() {
        assertTrue(!!subpage.$$('#showDiskResizeButton'));
        await crostiniBrowserProxy.resolvePromises(
            'getCrostiniDiskInfo',
            {succeeded: true, canResize: true, isUserChosenSize: true});
        subpage.$$('#showDiskResizeButton').click();

        await test_util.flushTasks();
        const dialog = subpage.$$('settings-crostini-disk-resize-dialog');
        assertTrue(!!dialog);
      });
    });

    suite('SubPagePortForwarding', function() {
      /** @type {?SettingsCrostiniPortForwarding} */
      let subpage;
      setup(async function() {
        crostiniBrowserProxy.portOperationSuccess = true;
        setCrostiniPrefs(true, {
          forwardedPorts: [
            {
              port_number: 5000,
              protocol_type: 0,
              label: 'Label1',
            },
            {
              port_number: 5001,
              protocol_type: 1,
              label: 'Label2',
            },
          ]
        });

        await test_util.flushTasks();
        settings.Router.getInstance().navigateTo(
            settings.routes.CROSTINI_PORT_FORWARDING);

        await test_util.flushTasks();
        subpage = crostiniPage.$$('settings-crostini-port-forwarding');
        assertTrue(!!subpage);
      });

      test('DisplayPorts', async function() {
        // Extra list item for the titles.
        assertEquals(
            3, subpage.shadowRoot.querySelectorAll('.list-item').length);
      });

      test('AddPortSuccess', async function() {
        await test_util.flushTasks();
        subpage = crostiniPage.$$('settings-crostini-port-forwarding');
        subpage.$$('#addPort cr-button').click();

        await test_util.flushTasks();
        subpage = subpage.$$('settings-crostini-add-port-dialog');
        const portNumberInput = subpage.$$('#portNumberInput');
        portNumberInput, focus();
        portNumberInput.value = '5002';
        portNumberInput, blur();
        assertEquals(portNumberInput.invalid, false);
        const portLabelInput = subpage.$$('#portLabelInput');
        portLabelInput.value = 'Some Label';
        subpage.$$('cr-dialog cr-button[id="continue"]').click();
        assertEquals(
            1, crostiniBrowserProxy.getCallCount('addCrostiniPortForward'));
      });

      test('AddPortFail', async function() {
        await test_util.flushTasks();
        subpage = crostiniPage.$$('settings-crostini-port-forwarding');
        subpage.$$('#addPort cr-button').click();

        await test_util.flushTasks();
        subpage = subpage.$$('settings-crostini-add-port-dialog');
        const portNumberInput = subpage.$$('#portNumberInput');
        const portLabelInput = subpage.$$('#portLabelInput');
        const continueButton = subpage.$$('cr-dialog cr-button[id="continue"]');

        assertEquals(portNumberInput.invalid, false);
        portNumberInput.focus();
        portNumberInput.value = '1023';
        continueButton.click();
        assertEquals(
            0, crostiniBrowserProxy.getCallCount('addCrostiniPortForward'));
        assertEquals(continueButton.disabled, true);
        assertEquals(portNumberInput.invalid, true);
        assertEquals(
            portNumberInput.errorMessage,
            loadTimeData.getString('crostiniPortForwardingAddError'));

        portNumberInput.value = '65536';
        assertEquals(continueButton.disabled, true);
        assertEquals(portNumberInput.invalid, true);
        assertEquals(
            portNumberInput.errorMessage,
            loadTimeData.getString('crostiniPortForwardingAddError'));

        portNumberInput.focus();
        portNumberInput.value = '5000';
        portNumberInput.blur();
        subpage.$$('cr-dialog cr-button[id="continue"]').click();
        assertEquals(continueButton.disabled, true);
        assertEquals(portNumberInput.invalid, true);
        assertEquals(
            portNumberInput.errorMessage,
            loadTimeData.getString('crostiniPortForwardingAddExisting'));

        portNumberInput.focus();
        portNumberInput.value = '1024';
        portNumberInput.blur();
        assertEquals(continueButton.disabled, false);
        assertEquals(portNumberInput.invalid, false);
        assertEquals(portNumberInput.errorMessage, '');
      });

      test('AddPortCancel', async function() {
        await test_util.flushTasks();
        subpage = crostiniPage.$$('settings-crostini-port-forwarding');
        subpage.$$('#addPort cr-button').click();

        await test_util.flushTasks();
        subpage = subpage.$$('settings-crostini-add-port-dialog');
        subpage.$$('cr-dialog cr-button[id="cancel"]').click();

        await test_util.flushTasks();
        subpage = crostiniPage.$$('settings-crostini-port-forwarding');
        assertTrue(!!subpage);
      });

      test('RemoveAllPorts', async function() {
        await test_util.flushTasks();
        subpage = crostiniPage.$$('settings-crostini-port-forwarding');
        subpage.$$('#showRemoveAllPortsMenu').click();

        await test_util.flushTasks();
        subpage.$$('#removeAllPortsButton').click();
        assertEquals(
            1,
            crostiniBrowserProxy.getCallCount('removeAllCrostiniPortForwards'));
      });

      test('RemoveSinglePort', async function() {
        await test_util.flushTasks();
        subpage = crostiniPage.$$('settings-crostini-port-forwarding');
        subpage.$$('#showRemoveSinglePortMenu0').click();
        await test_util.flushTasks();

        subpage.$$('#removeSinglePortButton').click();
        assertEquals(
            1, crostiniBrowserProxy.getCallCount('removeCrostiniPortForward'));
      });


      test('ActivateSinglePortSucess', async function() {
        assertFalse(subpage.$$('#errorToast').open);
        await test_util.flushTasks();
        subpage = crostiniPage.$$('settings-crostini-port-forwarding');
        subpage.$$('#toggleActivationButton0').click();

        await test_util.flushTasks();
        assertEquals(
            1,
            crostiniBrowserProxy.getCallCount('activateCrostiniPortForward'));
        assertFalse(subpage.$$('#errorToast').open);
      });

      test('ActivateSinglePortFail', async function() {
        await test_util.flushTasks();
        crostiniBrowserProxy.portOperationSuccess = false;
        assertFalse(subpage.$$('#errorToast').open);
        subpage = crostiniPage.$$('settings-crostini-port-forwarding');
        const crToggle = subpage.$$('#toggleActivationButton1');
        assertEquals(crToggle.checked, false);
        crToggle.click();

        await test_util.flushTasks();
        assertEquals(
            1,
            crostiniBrowserProxy.getCallCount('activateCrostiniPortForward'));
        assertEquals(crToggle.checked, false);
        assertTrue(subpage.$$('#errorToast').open);
      });

      test('DeactivateSinglePort', async function() {
        await test_util.flushTasks();
        subpage = crostiniPage.$$('settings-crostini-port-forwarding');
        const crToggle = subpage.$$('#toggleActivationButton0');
        crToggle.checked = true;
        crToggle.click();

        await test_util.flushTasks();
        assertEquals(
            1,
            crostiniBrowserProxy.getCallCount('deactivateCrostiniPortForward'));
      });

      test('ActivePortsChanged', async function() {
        setCrostiniPrefs(true, {
          forwardedPorts: [
            {
              port_number: 5000,
              protocol_type: 0,
              label: 'Label1',
            },
          ]
        });
        const crToggle = subpage.$$('#toggleActivationButton0');

        cr.webUIListenerCallback(
            'crostini-port-forwarder-active-ports-changed',
            [{'port_number': 5000, 'protocol_type': 0}]);
        await test_util.flushTasks();
        assertTrue(crToggle.checked);

        cr.webUIListenerCallback(
            'crostini-port-forwarder-active-ports-changed', []);
        await test_util.flushTasks();
        assertFalse(crToggle.checked);
      });

      test('PortPrefsChange', async function() {
        setCrostiniPrefs(true, {
          forwardedPorts: [
            {
              port_number: 5000,
              protocol_type: 0,
              label: 'Label1',
            },
            {
              port_number: 5001,
              protocol_type: 0,
              label: 'Label1',
            },
          ]
        });
        assertEquals(
            3, subpage.shadowRoot.querySelectorAll('.list-item').length);
        setCrostiniPrefs(true, {
          forwardedPorts: [
            {
              port_number: 5000,
              protocol_type: 0,
              label: 'Label1',
            },
            {
              port_number: 5001,
              protocol_type: 0,
              label: 'Label1',
            },
            {
              port_number: 5002,
              protocol_type: 0,
              label: 'Label1',
            },
          ]
        });
        assertEquals(
            4, subpage.shadowRoot.querySelectorAll('.list-item').length);
        setCrostiniPrefs(true, {forwardedPorts: []});
        assertEquals(
            0, subpage.shadowRoot.querySelectorAll('.list-item').length);
      });

      test('CrostiniStopAndStart', async function() {
        const crToggle = subpage.$$('#toggleActivationButton0');
        assertFalse(crToggle.disabled);

        cr.webUIListenerCallback('crostini-status-changed', false);
        assertTrue(crToggle.disabled);

        cr.webUIListenerCallback('crostini-status-changed', true);
        assertFalse(crToggle.disabled);
      });
    });

    suite('DiskResize', async function() {
      let dialog;
      /**
       * Helper function to assert that the expected block is visible and the
       * others are not.
       * @param {!string} selector
       */
      function assertVisibleBlockIs(selector) {
        const selectors =
            ['#unsupported', '#resize-block', '#error', '#loading'];

        assertTrue(isVisible(dialog.$$(selector)));
        selectors.filter(s => s !== selector).forEach(s => {
          assertFalse(isVisible(dialog.$$(s)));
        });
      }

      const ticks = [
        {label: 'label 0', value: 0, ariaLabel: 'label 0'},
        {label: 'label 10', value: 10, ariaLabel: 'label 10'},
        {label: 'label 100', value: 100, ariaLabel: 'label 100'}
      ];

      const resizeableData = {
        succeeded: true,
        canResize: true,
        isUserChosenSize: true,
        isLowSpaceAvailable: false,
        defaultIndex: 2,
        ticks: ticks
      };

      const sparseDiskData = {
        succeeded: true,
        canResize: true,
        isUserChosenSize: false,
        isLowSpaceAvailable: false,
        defaultIndex: 2,
        ticks: ticks
      };

      async function clickShowDiskResize(userChosen) {
        console.log('Running clickShowDiskResize');
        console.log('Awaiting test_util.flushTasks');
        await test_util.flushTasks();
        console.log('Awaiting getDiskInfo');
        await crostiniBrowserProxy.resolvePromises('getCrostiniDiskInfo', {
          succeeded: true,
          canResize: true,
          isUserChosenSize: userChosen,
          ticks: ticks,
          defaultIndex: 2
        });
        subpage.$$('#showDiskResizeButton').click();
        console.log('Awaiting test_util.flushTasks');
        await test_util.flushTasks();
        dialog = subpage.$$('settings-crostini-disk-resize-dialog');

        if (userChosen) {
          // We should be on the loading page but unable to kick off a resize
          // yet.
          assertTrue(!!dialog.$$('#loading'));
          assertTrue(dialog.$$('#resize').disabled);
        }
      }

      setup(async function() {
        console.log('Running setup');
        assertTrue(!!subpage.$$('#showDiskResizeButton'));
        const subtext = subpage.$$('#diskSizeDescription');
        console.log('Awaiting test_util.flushTasks');
        await test_util.flushTasks();
      });

      test('ResizeUnsupported', async function() {
        console.log(`Starting test ${this.test.title}`);
        await crostiniBrowserProxy.resolvePromises(
            'getCrostiniDiskInfo', {succeeded: true, canResize: false});
        assertFalse(isVisible(subpage.$$('#showDiskResizeButton')));
        assertEquals(
            subpage.$$('#diskSizeDescription').innerText,
            loadTimeData.getString('crostiniDiskResizeNotSupportedSubtext'));
        console.log('Rejecting promises.');
        await crostiniBrowserProxy.rejectAllPromises(
            ['getCrostiniDiskInfo', 'resizeCrostiniDisk']);
        console.log(`Finished test ${this.test.title}`);
      });

      test('ResizeButtonAndSubtextCorrectlySet', async function() {
        console.log(`Starting test ${this.test.title}`);
        await crostiniBrowserProxy.resolvePromises(
            'getCrostiniDiskInfo', resizeableData);
        const button = subpage.$$('#showDiskResizeButton');
        const subtext = subpage.$$('#diskSizeDescription');

        assertEquals(
            button.innerText,
            loadTimeData.getString('crostiniDiskResizeShowButton'));
        assertEquals(subtext.innerText, 'label 100');
        console.log('Rejecting promises.');
        await crostiniBrowserProxy.rejectAllPromises(
            ['getCrostiniDiskInfo', 'resizeCrostiniDisk']);
        console.log(`Finished test ${this.test.title}`);
      });

      test('ReserveSizeButtonAndSubtextCorrectlySet', async function() {
        console.log(`Starting test ${this.test.title}`);
        await crostiniBrowserProxy.resolvePromises(
            'getCrostiniDiskInfo', sparseDiskData);
        const button = subpage.$$('#showDiskResizeButton');
        const subtext = subpage.$$('#diskSizeDescription');

        assertEquals(
            button.innerText,
            loadTimeData.getString('crostiniDiskReserveSizeButton'));
        assertEquals(
            subtext.innerText,
            loadTimeData.getString(
                'crostiniDiskResizeDynamicallyAllocatedSubtext'));
        console.log('Rejecting promises.');
        await crostiniBrowserProxy.rejectAllPromises(
            ['getCrostiniDiskInfo', 'resizeCrostiniDisk']);
        console.log(`Finished test ${this.test.title}`);
      });

      test('ResizeRecommendationShownCorrectly', async function() {
        console.log(`Starting test ${this.test.title}`);
        await clickShowDiskResize(true);
        const diskInfo = resizeableData;
        console.log('Awaiting getCrostiniDiskInfo');
        await crostiniBrowserProxy.resolvePromises(
            'getCrostiniDiskInfo', diskInfo);

        assertTrue(isVisible(dialog.$$('#recommended-size')));
        assertFalse(isVisible(dialog.$$('#recommended-size-warning')));
        console.log('Rejecting promises.');
        await crostiniBrowserProxy.rejectAllPromises(
            ['getCrostiniDiskInfo', 'resizeCrostiniDisk']);
        console.log(`Finished test ${this.test.title}`);
      });

      test('ResizeRecommendationWarningShownCorrectly', async function() {
        console.log(`Starting test ${this.test.title}`);
        await clickShowDiskResize(true);
        const diskInfo = resizeableData;
        diskInfo.isLowSpaceAvailable = true;
        console.log('Awaiting getCrostiniDiskInfo');
        await crostiniBrowserProxy.resolvePromises(
            'getCrostiniDiskInfo', diskInfo);

        assertFalse(isVisible(dialog.$$('#recommended-size')));
        assertTrue(isVisible(dialog.$$('#recommended-size-warning')));
        console.log('Rejecting promises.');
        await crostiniBrowserProxy.rejectAllPromises(
            ['getCrostiniDiskInfo', 'resizeCrostiniDisk']);
        console.log(`Finished test ${this.test.title}`);
      });

      test('MessageShownIfErrorAndCanRetry', async function() {
        console.log(`Starting test ${this.test.title}`);
        await clickShowDiskResize(true);
        console.log('Awaiting getCrostiniDiskInfo');
        await crostiniBrowserProxy.resolvePromises(
            'getCrostiniDiskInfo', {succeeded: false, isUserChosenSize: true});

        // We failed, should have a retry button.
        let button = dialog.$$('#retry');
        assertVisibleBlockIs('#error');
        assertTrue(isVisible(button));
        assertTrue(dialog.$$('#resize').disabled);
        assertFalse(dialog.$$('#cancel').disabled);

        // Back to the loading screen.
        button.click();
        console.log('Awaiting test_util.flushTasks');
        await test_util.flushTasks();
        assertVisibleBlockIs('#loading');
        assertTrue(dialog.$$('#resize').disabled);
        assertFalse(dialog.$$('#cancel').disabled);

        // And failure page again.
        console.log('Awaiting getCrostiniDiskInfo');
        await crostiniBrowserProxy.rejectPromises('getCrostiniDiskInfo');
        button = dialog.$$('#retry');
        assertTrue(isVisible(button));
        assertVisibleBlockIs('#error');
        assertTrue(dialog.$$('#resize').disabled);
        assertTrue(dialog.$$('#resize').disabled);
        assertFalse(dialog.$$('#cancel').disabled);

        console.log('Rejecting promises.');
        await crostiniBrowserProxy.rejectAllPromises(
            ['getCrostiniDiskInfo', 'resizeCrostiniDisk']);
        console.log(`Finished test ${this.test.title}`);
      });

      test('MessageShownIfCannotResize', async function() {
        console.log(`Starting test ${this.test.title}`);
        await clickShowDiskResize(true);
        console.log('Awaiting getCrostiniDiskInfo');
        await crostiniBrowserProxy.resolvePromises(
            'getCrostiniDiskInfo',
            {succeeded: true, canResize: false, isUserChosenSize: true});
        assertVisibleBlockIs('#unsupported');
        assertTrue(dialog.$$('#resize').disabled);
        assertFalse(dialog.$$('#cancel').disabled);
        console.log('Rejecting promises.');
        await crostiniBrowserProxy.rejectAllPromises(
            ['getCrostiniDiskInfo', 'resizeCrostiniDisk']);
        console.log(`Finished test ${this.test.title}`);
      });

      test('ResizePageShownIfCanResize', async function() {
        console.log(`Starting test ${this.test.title}`);
        await clickShowDiskResize(true);
        console.log('Awaiting getCrostiniDiskInfo');
        await crostiniBrowserProxy.resolvePromises(
            'getCrostiniDiskInfo', resizeableData);
        assertVisibleBlockIs('#resize-block');

        assertEquals(ticks[0].label, dialog.$$('#label-begin').innerText);
        assertEquals(ticks[2].label, dialog.$$('#label-end').innerText);
        assertEquals(2, dialog.$$('#diskSlider').value);

        assertFalse(dialog.$$('#resize').disabled);
        assertFalse(dialog.$$('#cancel').disabled);
        console.log('Rejecting promises.');
        await crostiniBrowserProxy.rejectAllPromises(
            ['getCrostiniDiskInfo', 'resizeCrostiniDisk']);
        console.log(`Finished test ${this.test.title}`);
      });

      test('InProgressResizing', async function() {
        console.log(`Starting test ${this.test.title}`);
        await clickShowDiskResize(true);
        console.log('Awaiting getCrostiniDiskInfo');
        await crostiniBrowserProxy.resolvePromises(
            'getCrostiniDiskInfo', resizeableData);
        const button = dialog.$$('#resize');
        button.click();
        console.log('Awaiting test_util.flushTasks');
        await test_util.flushTasks();
        assertTrue(button.disabled);
        assertFalse(isVisible(dialog.$$('#done')));
        assertTrue(isVisible(dialog.$$('#resizing')));
        assertFalse(isVisible(dialog.$$('#resize-error')));
        assertTrue(dialog.$$('#cancel').disabled);
        console.log('Rejecting promises.');
        await crostiniBrowserProxy.rejectAllPromises(
            ['getCrostiniDiskInfo', 'resizeCrostiniDisk']);
        console.log(`Finished test ${this.test.title}`);
      });

      test('ErrorResizing', async function() {
        console.log(`Starting test ${this.test.title}`);
        await clickShowDiskResize(true);
        console.log('Awaiting getCrostiniDiskInfo');
        await crostiniBrowserProxy.resolvePromises(
            'getCrostiniDiskInfo', resizeableData);
        const button = dialog.$$('#resize');
        button.click();
        console.log('Awaiting resizeCrostiniDisk');
        await crostiniBrowserProxy.resolvePromises('resizeCrostiniDisk', false);
        assertFalse(button.disabled);
        assertFalse(isVisible(dialog.$$('#done')));
        assertFalse(isVisible(dialog.$$('#resizing')));
        assertTrue(isVisible(dialog.$$('#resize-error')));
        assertFalse(dialog.$$('#cancel').disabled);
        console.log('Rejecting promises.');
        await crostiniBrowserProxy.rejectAllPromises(
            ['getCrostiniDiskInfo', 'resizeCrostiniDisk']);
        console.log(`Finished test ${this.test.title}`);
      });

      test('SuccessResizing', async function() {
        console.log(`Starting test ${this.test.title}`);
        await clickShowDiskResize(true);
        console.log('Awaiting getCrostiniDiskInfo');
        await crostiniBrowserProxy.resolvePromises(
            'getCrostiniDiskInfo', resizeableData);
        const button = dialog.$$('#resize');
        button.click();
        console.log('Awaiting resizeCrostiniDisk');
        await crostiniBrowserProxy.resolvePromises('resizeCrostiniDisk', true);
        // Dialog should close itself.
        console.log('Awaiting close');
        await test_util.eventToPromise('close', dialog);
        console.log('Rejecting promises.');
        await crostiniBrowserProxy.rejectAllPromises(
            ['getCrostiniDiskInfo', 'resizeCrostiniDisk']);
        console.log(`Finished test ${this.test.title}`);
      });

      test('DiskResizeConfirmationDialogShownAndAccepted', async function() {
        console.log(`Starting test ${this.test.title}`);
        await crostiniBrowserProxy.resolvePromises(
            'getCrostiniDiskInfo', sparseDiskData);
        console.log('Awaiting show');
        await clickShowDiskResize(false);
        // Dismiss confirmation.
        let confirmationDialog =
            subpage.$$('settings-crostini-disk-resize-confirmation-dialog');
        assertTrue(isVisible(confirmationDialog.$$('#continue')));
        assertTrue(isVisible(confirmationDialog.$$('#cancel')));
        confirmationDialog.$$('#continue').click();
        console.log('Awaiting close');
        await test_util.eventToPromise('close', confirmationDialog);
        assertFalse(isVisible(confirmationDialog));

        dialog = subpage.$$('settings-crostini-disk-resize-dialog');
        assertTrue(!!dialog);
        assertTrue(isVisible(dialog.$$('#resize')));
        assertTrue(isVisible(dialog.$$('#cancel')));

        // Cancel main resize dialog.
        dialog.$$('#cancel').click();
        console.log('Awaiting close');
        await test_util.eventToPromise('close', dialog);
        assertFalse(isVisible(dialog));

        // On another click, confirmation dialog should be shown again.
        console.log('Awaiting click show');
        await clickShowDiskResize(false);
        confirmationDialog =
            subpage.$$('settings-crostini-disk-resize-confirmation-dialog');
        assertTrue(isVisible(confirmationDialog.$$('#continue')));
        confirmationDialog.$$('#continue').click();
        console.log('Awaiting close');
        await test_util.eventToPromise('close', confirmationDialog);

        // Main dialog should show again.
        dialog = subpage.$$('settings-crostini-disk-resize-dialog');
        assertTrue(!!dialog);
        assertTrue(isVisible(dialog.$$('#resize')));
        assertTrue(isVisible(dialog.$$('#cancel')));
        console.log('Rejecting promises.');
        await crostiniBrowserProxy.rejectAllPromises(
            ['getCrostiniDiskInfo', 'resizeCrostiniDisk']);
        console.log(`Finished test ${this.test.title}`);
      });

      test('DiskResizeConfirmationDialogShownAndCanceled', async function() {
        console.log(`Starting test ${this.test.title}`);
        await crostiniBrowserProxy.resolvePromises(
            'getCrostiniDiskInfo', sparseDiskData);
        console.log('Awaiting click show');
        await clickShowDiskResize(false);

        const confirmationDialog =
            subpage.$$('settings-crostini-disk-resize-confirmation-dialog');
        assertTrue(isVisible(confirmationDialog.$$('#continue')));
        assertTrue(isVisible(confirmationDialog.$$('#cancel')));
        confirmationDialog.$$('#cancel').click();
        console.log('Awaiting confirmationDialog close');
        await test_util.eventToPromise('close', confirmationDialog);

        assertFalse(!!subpage.$$('settings-crostini-disk-resize-dialog'));
        console.log('Rejecting promises.');
        await crostiniBrowserProxy.rejectAllPromises(
            ['getCrostiniDiskInfo', 'resizeCrostiniDisk']);
        console.log(`Finished test ${this.test.title}`);
      });
    });
  });

  suite('SubPageSharedPaths', function() {
    let subpage;

    setup(async function() {
      setCrostiniPrefs(
          true, {sharedPaths: {path1: ['termina'], path2: ['termina']}});

      await test_util.flushTasks();
      settings.Router.getInstance().navigateTo(
          settings.routes.CROSTINI_SHARED_PATHS);

      await test_util.flushTasks();
      subpage = crostiniPage.$$('settings-crostini-shared-paths');
      assertTrue(!!subpage);
    });

    test('Sanity', function() {
      assertEquals(
          3, subpage.shadowRoot.querySelectorAll('.settings-box').length);
      assertEquals(2, subpage.shadowRoot.querySelectorAll('.list-item').length);
    });

    test('Remove', async function() {
      assertFalse(subpage.$.crostiniInstructionsRemove.hidden);
      assertFalse(subpage.$.crostiniList.hidden);
      assertTrue(subpage.$.crostiniListEmpty.hidden);
      assertTrue(!!subpage.$$('.list-item cr-icon-button'));

      {
        // Remove first shared path, still one left.
        subpage.$$('.list-item cr-icon-button').click();
        const [vmName, path] =
            await crostiniBrowserProxy.whenCalled('removeCrostiniSharedPath');
        assertEquals('termina', vmName);
        assertEquals('path1', path);
        setCrostiniPrefs(true, {sharedPaths: {path2: ['termina']}});
      }

      await test_util.flushTasks();
      Polymer.dom.flush();
      assertEquals(1, subpage.shadowRoot.querySelectorAll('.list-item').length);
      assertFalse(subpage.$.crostiniInstructionsRemove.hidden);

      {
        // Remove remaining shared path, none left.
        crostiniBrowserProxy.resetResolver('removeCrostiniSharedPath');
        subpage.$$('.list-item cr-icon-button').click();
        const [vmName, path] =
            await crostiniBrowserProxy.whenCalled('removeCrostiniSharedPath');
        assertEquals('termina', vmName);
        assertEquals('path2', path);
        setCrostiniPrefs(true, {sharedPaths: {}});
      }

      await test_util.flushTasks();
      Polymer.dom.flush();
      assertEquals(0, subpage.shadowRoot.querySelectorAll('.list-item').length);
      // Verify remove instructions are hidden, and empty list message
      // is shown.
      assertTrue(subpage.$.crostiniInstructionsRemove.hidden);
      assertTrue(subpage.$.crostiniList.hidden);
      assertFalse(subpage.$.crostiniListEmpty.hidden);
    });

    test('RemoveFailedRetry', async function() {
      // Remove shared path fails.
      crostiniBrowserProxy.removeSharedPathResult = false;
      subpage.$$('.list-item cr-icon-button').click();

      await crostiniBrowserProxy.whenCalled('removeCrostiniSharedPath');
      Polymer.dom.flush();
      assertTrue(subpage.$$('#removeSharedPathFailedDialog').open);

      // Click retry and make sure 'removeCrostiniSharedPath' is called
      // and dialog is closed/removed.
      crostiniBrowserProxy.removeSharedPathResult = true;
      subpage.$$('#removeSharedPathFailedDialog')
          .querySelector('.action-button')
          .click();
      await crostiniBrowserProxy.whenCalled('removeCrostiniSharedPath');
      assertFalse(!!subpage.$$('#removeSharedPathFailedDialog'));
    });
  });

  suite('SubPageSharedUsbDevices', function() {
    let subpage;

    setup(async function() {
      setCrostiniPrefs(true, {
        sharedUsbDevices: [
          {shared: true, guid: '0001', name: 'usb_dev1'},
          {shared: false, guid: '0002', name: 'usb_dev2'},
          {shared: true, guid: '0003', name: 'usb_dev3'}
        ]
      });

      await test_util.flushTasks();
      settings.Router.getInstance().navigateTo(
          settings.routes.CROSTINI_SHARED_USB_DEVICES);

      await test_util.flushTasks();
      subpage = crostiniPage.$$('settings-crostini-shared-usb-devices');
      assertTrue(!!subpage);
    });

    test('USB devices are shown', function() {
      assertEquals(3, subpage.shadowRoot.querySelectorAll('.toggle').length);
    });

    test('USB shared state is updated by toggling', async function() {
      assertTrue(!!subpage.$$('.toggle'));
      subpage.$$('.toggle').click();

      await test_util.flushTasks();
      Polymer.dom.flush();

      const args =
          await crostiniBrowserProxy.whenCalled('setCrostiniUsbDeviceShared');
      assertEquals('0001', args[0]);
      assertEquals(false, args[1]);

      // Simulate a change in the underlying model.
      cr.webUIListenerCallback('crostini-shared-usb-devices-changed', [
        {shared: true, guid: '0001', name: 'usb_dev1'},
      ]);
      Polymer.dom.flush();
      assertEquals(1, subpage.shadowRoot.querySelectorAll('.toggle').length);
    });
  });
});
