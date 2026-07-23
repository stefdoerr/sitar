/*
 * Sitar Sympathetic Resonance — MOD modgui controller.
 *
 * The 13 string frequencies are computed entirely in the DSP from two LV2
 * enum parameters: `scale` (0..N-1 indexing the C++ scale table) and
 * `root_note` (0..11 indexing C2..B2). This script's job is only:
 *
 *   - When the user picks a scale or root in the pedalboard dropdowns,
 *     translate the dropdown value to the matching integer index and write
 *     it to the LV2 port (`funcs.set_port_value`).
 *   - When the host changes scale/root externally (settings popup, preset
 *     recall, MIDI CC), update the dropdowns to reflect the new index.
 *
 * The arrays below MUST stay in sync with kScales[] / kRootHz[] in
 * SitarPlugin.cpp — they're a parallel lookup table for the integer indices.
 */

function (event, funcs) {

    // Same order as kScales[] in SitarPlugin.cpp.
    var SCALE_KEYS = [
        'major', 'natural-minor', 'harmonic-minor', 'melodic-minor',
        'dorian', 'phrygian', 'lydian', 'mixolydian', 'locrian',
        'major-pentatonic', 'minor-pentatonic', 'blues',
        'pythagorean', 'chromatic-12tet',
        'makam-rast', 'makam-ussak', 'makam-hicaz', 'makam-saba',
        'raga-yaman', 'raga-bhairav', 'raga-bhairavi',
        'raga-todi', 'raga-marwa', 'raga-malkauns'
    ];

    // Same order as kRootHz[] in SitarPlugin.cpp. Values are the Hz strings
    // used as the <option value="..."> for the root dropdown. The Hz values
    // are pitch classes in the reference octave (C3=130.81 ... B3=246.94);
    // the OCT knob in the plugin chooses the absolute octave.
    var ROOT_VALUES = [
        '130.81', '138.59', '146.83', '155.56', '164.81', '174.61',
        '185.00', '196.00', '207.65', '220.00', '233.08', '246.94'
    ];

    // Same order as kStereoModes[] in SitarPlugin.cpp.
    var STEREO_KEYS = [
        'mono', 'linear-narrow', 'linear', 'wide-narrow', 'wide'
    ];

    // --- user-scale library helpers (8 positional slots) ---
    // Mirrors sitar::parseUserScales() in UserScale.hpp: one scale per line,
    // "Name | i1, i2, ...", line i -> slot i (positional). SCALE port indices
    // 0..BUILTINS-1 are the built-ins above; BUILTINS+i selects user slot i.
    var NUSER = 8, BUILTINS = SCALE_KEYS.length;

    function parseLibrary(text) {           // -> array[8] of {name, ivals} or null
        var slots = new Array(NUSER); for (var i=0;i<NUSER;i++) slots[i]=null;
        (text||'').split('\n').forEach(function(line, i){
            if (i>=NUSER) return; var bar=line.indexOf('|'); if (bar<0) return;
            var name=line.slice(0,bar).trim(); if (!name) return;
            slots[i]={name:name, ivals:line.slice(bar+1).trim()};
        });
        return slots;
    }
    function serializeLibrary(slots) {
        return slots.map(function(s){ return s ? (s.name+' | '+s.ivals) : ''; })
                    .join('\n').replace(/\n+$/,'');
    }
    function refreshScaleDropdown(icon, slots) {
        var $scale = icon.find('[mod-role="sitar-scale"]');
        $scale.find('optgroup[label="User"], option.sitar-user-opt').remove();
        var $g = jQuery('<optgroup class="sitar-user-grp" label="User"></optgroup>');
        for (var i=0;i<NUSER;i++) if (slots[i])
            $g.append('<option class="sitar-user-opt" value="user'+i+'">'+slots[i].name+'</option>');
        if ($g.children().length) $scale.append($g);
    }
    // Rebuild the editor's slot <select> (User 1..8, showing name if filled).
    // Preserves the current selection unless keepValue is false.
    function populateSlotSelect($slotSel, slots, keepValue) {
        var cur = keepValue ? $slotSel.val() : null;
        $slotSel.empty();
        for (var i = 0; i < NUSER; i++) {
            var label = 'User ' + (i + 1) + (slots[i] ? ': ' + slots[i].name : ' (empty)');
            $slotSel.append('<option value="' + i + '">' + label + '</option>');
        }
        $slotSel.val(cur != null ? cur : '0');
    }
    // Fill the name/intervals editor fields from the currently selected slot.
    function fillEditorFields(icon, slots) {
        var slotIdx = parseInt(icon.find('[mod-role="sitar-edit-slot"]').val(), 10) || 0;
        var s = slots[slotIdx];
        icon.find('[mod-role="sitar-edit-name"]').val(s ? s.name : '');
        icon.find('[mod-role="sitar-edit-ivals"]').val(s ? s.ivals : '');
    }

    function scaleKeyToIndex(key) {
        if (typeof key === 'string' && key.indexOf('user') === 0) {
            var uidx = parseInt(key.slice(4), 10);
            if (!isNaN(uidx) && uidx >= 0 && uidx < NUSER) return BUILTINS + uidx;
        }
        var idx = SCALE_KEYS.indexOf(key);
        return idx < 0 ? 0 : idx;
    }

    function rootValueToIndex(value) {
        var idx = ROOT_VALUES.indexOf(value);
        return idx < 0 ? 0 : idx;
    }

    function stereoKeyToIndex(key) {
        var idx = STEREO_KEYS.indexOf(key);
        return idx < 0 ? 3 : idx; // default = 'wide-narrow'
    }

    if (event.type === 'start') {
        var icon = event.icon;
        var $scale  = icon.find('[mod-role="sitar-scale"]');
        var $root   = icon.find('[mod-role="sitar-root"]');
        var $stereo = icon.find('[mod-role="sitar-stereo"]');

        // Suppress re-emitting set_port_value when we update the <select> in
        // response to a 'change' event coming back from the host.
        icon.data('sitar-suppress-emit', false);

        $scale.on('change', function () {
            if (icon.data('sitar-suppress-emit')) return;
            funcs.set_port_value('scale', scaleKeyToIndex(this.value));
        });
        $root.on('change', function () {
            if (icon.data('sitar-suppress-emit')) return;
            funcs.set_port_value('root_note', rootValueToIndex(this.value));
        });
        $stereo.on('change', function () {
            if (icon.data('sitar-suppress-emit')) return;
            funcs.set_port_value('stereo_mode', stereoKeyToIndex(this.value));
        });

        // --- User-scale editor: discover the "userscales" patch parameter's
        // URI from the parameters list (robust across stable/beta builds),
        // parse its current value into the 8-slot library, and wire the
        // editor overlay + dynamic "User" group in the SCALE dropdown.
        var $editOpen  = icon.find('[mod-role="sitar-edit-open"]');
        var $editClose = icon.find('[mod-role="sitar-edit-close"]');
        var $editor    = icon.find('[mod-role="sitar-editor"]');
        var $slotSel   = icon.find('[mod-role="sitar-edit-slot"]');
        var $nameInp   = icon.find('[mod-role="sitar-edit-name"]');
        var $ivalsInp  = icon.find('[mod-role="sitar-edit-ivals"]');
        var $saveBtn   = icon.find('[mod-role="sitar-edit-save"]');
        var $clearBtn  = icon.find('[mod-role="sitar-edit-clear"]');

        var scalesUri = null;
        var scalesValue = '';
        (event.parameters || []).forEach(function (p) {
            if (p && p.uri && p.uri.indexOf('#userscales') !== -1) {
                scalesUri = p.uri;
                if (p.value != null) scalesValue = p.value;
            }
        });
        icon.data('sitar-userscales-uri', scalesUri);

        var lib = parseLibrary(scalesValue);
        icon.data('sitar-lib', lib);
        populateSlotSelect($slotSel, lib, false);
        fillEditorFields(icon, lib);
        refreshScaleDropdown(icon, lib);

        $editOpen.on('click', function () {
            fillEditorFields(icon, icon.data('sitar-lib'));
            $editor.show();
        });
        $editClose.on('click', function () {
            $editor.hide();
        });
        $slotSel.on('change', function () {
            fillEditorFields(icon, icon.data('sitar-lib'));
        });
        $saveBtn.on('click', function () {
            var uri = icon.data('sitar-userscales-uri');
            if (!uri) return;
            var slotIdx = parseInt($slotSel.val(), 10) || 0;
            var curLib = icon.data('sitar-lib');
            var name  = ($nameInp.val()  || '').trim();
            var ivals = ($ivalsInp.val() || '').trim();
            curLib[slotIdx] = name ? { name: name, ivals: ivals } : null;
            icon.data('sitar-lib', curLib);
            funcs.patch_set(uri, 's', serializeLibrary(curLib));
            populateSlotSelect($slotSel, curLib, true);
            refreshScaleDropdown(icon, curLib);
        });
        $clearBtn.on('click', function () {
            var uri = icon.data('sitar-userscales-uri');
            if (!uri) return;
            var slotIdx = parseInt($slotSel.val(), 10) || 0;
            var curLib = icon.data('sitar-lib');
            curLib[slotIdx] = null;
            icon.data('sitar-lib', curLib);
            funcs.patch_set(uri, 's', serializeLibrary(curLib));
            populateSlotSelect($slotSel, curLib, true);
            fillEditorFields(icon, curLib);
            refreshScaleDropdown(icon, curLib);
        });
        return;
    }

    if (event.type === 'change') {
        // Sync the dropdowns when scale/root_note/stereo_mode change externally
        // (settings popup, preset recall, automation). Setting <select>.val
        // via jQuery does NOT fire a 'change' event, so this won't loop back
        // to our handler, but we set a guard flag anyway to be safe.
        var icon = event.icon;

        // An incoming value for the "userscales" patch parameter arrives as a
        // 'change' event carrying a `uri` (not a port `symbol`) — e.g. preset
        // recall or an edit made from another client. Re-parse the library and
        // refresh the SCALE dropdown + editor fields to match.
        if (event.uri && event.uri.indexOf('#userscales') !== -1)
        {
            var newLib = parseLibrary(event.value != null ? event.value : '');
            icon.data('sitar-lib', newLib);
            populateSlotSelect(icon.find('[mod-role="sitar-edit-slot"]'), newLib, true);
            fillEditorFields(icon, newLib);
            refreshScaleDropdown(icon, newLib);
            return;
        }

        if (event.symbol === 'scale')
        {
            var idx = Math.round(event.value) | 0;
            if (idx < 0) idx = 0;
            icon.data('sitar-suppress-emit', true);
            if (idx >= BUILTINS)
            {
                icon.find('[mod-role="sitar-scale"]').val('user' + (idx - BUILTINS));
            }
            else
            {
                if (idx >= SCALE_KEYS.length) idx = SCALE_KEYS.length - 1;
                icon.find('[mod-role="sitar-scale"]').val(SCALE_KEYS[idx]);
            }
            icon.data('sitar-suppress-emit', false);
        }
        else if (event.symbol === 'root_note')
        {
            var idx = Math.round(event.value) | 0;
            if (idx < 0) idx = 0;
            if (idx >= ROOT_VALUES.length) idx = ROOT_VALUES.length - 1;
            icon.data('sitar-suppress-emit', true);
            icon.find('[mod-role="sitar-root"]').val(ROOT_VALUES[idx]);
            icon.data('sitar-suppress-emit', false);
        }
        else if (event.symbol === 'stereo_mode')
        {
            var idx = Math.round(event.value) | 0;
            if (idx < 0) idx = 0;
            if (idx >= STEREO_KEYS.length) idx = STEREO_KEYS.length - 1;
            icon.data('sitar-suppress-emit', true);
            icon.find('[mod-role="sitar-stereo"]').val(STEREO_KEYS[idx]);
            icon.data('sitar-suppress-emit', false);
        }
        return;
    }
}
