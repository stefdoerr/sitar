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

    function scaleKeyToIndex(key) {
        var idx = SCALE_KEYS.indexOf(key);
        return idx < 0 ? 0 : idx;
    }

    function rootValueToIndex(value) {
        var idx = ROOT_VALUES.indexOf(value);
        return idx < 0 ? 0 : idx;
    }

    if (event.type === 'start') {
        var icon = event.icon;
        var $scale = icon.find('[mod-role="sitar-scale"]');
        var $root  = icon.find('[mod-role="sitar-root"]');

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
        return;
    }

    if (event.type === 'change') {
        // Sync the dropdowns when scale/root_note change externally (settings
        // popup, preset recall, automation). Setting <select>.val via jQuery
        // does NOT fire a 'change' event, so this won't loop back to our
        // handler, but we set a guard flag anyway to be safe.
        var icon = event.icon;
        if (event.symbol === 'scale')
        {
            var idx = Math.round(event.value) | 0;
            if (idx < 0) idx = 0;
            if (idx >= SCALE_KEYS.length) idx = SCALE_KEYS.length - 1;
            icon.data('sitar-suppress-emit', true);
            icon.find('[mod-role="sitar-scale"]').val(SCALE_KEYS[idx]);
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
        return;
    }
}
