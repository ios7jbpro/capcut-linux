'use strict';
var reported = 0;
function isCapCutOverlay(w) {
    if (!w || !w.x11Client || w.caption !== 'CapCut' ||
        String(w.windowClass).indexOf('steam_proton') < 0 ||
        !(w.dialog || w.utility) || w.modal || w.hasDecoration) return false;
    var owner = w.transientFor();
    return owner && owner.caption === 'CapCut' && owner.normalWindow &&
        owner.pid === w.pid;
}
function suppressOverlayAnimation(w) {
    if (!isCapCutOverlay(w)) return;
    var opened = effect.grab(w, Effect.WindowAddedGrabRole, true);
    var closed = effect.grab(w, Effect.WindowClosedGrabRole, true);
    if (reported < 8) {
        print('capcut-overlay-instant: open=' + opened + ' close=' + closed +
              ' geometry=' + w.width + 'x' + w.height);
        ++reported;
    }
}
effects.windowAdded.connect(suppressOverlayAnimation);
effects.windowClosed.connect(suppressOverlayAnimation);
for (var i = 0; i < effects.stackingOrder.length; ++i)
    suppressOverlayAnimation(effects.stackingOrder[i]);
