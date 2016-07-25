'use strict';

var amino = require('../../main.js');

amino.start(function (core, stage) {
    //root
    var root = new amino.Group().id('group');

    root.acceptsMouseEvents = true;
    stage.setRoot(root);

    //rect
    var rect = new amino.Rect().w(100).h(50).fill('#ccccff').x(50).y(50).id('clickrect');

    rect.acceptsMouseEvents = true;
    root.add(rect);

    core.on('press', rect, function () {
        rect.fill('#ffffff');
        console.log('pressed');
    });

    core.on('release', rect, function () {
        rect.fill('#ccccff');
        console.log('released');
    });

    core.on('click', rect, function () {
        console.log('clicked');
    });

    //rect 2
    var r2 = new amino.Rect().w(30).h(30).fill('#ff6666').x(300).y(50).id('dragrect');

    root.add(r2);
    r2.acceptsMouseEvents = true;

    core.on('drag', r2, function (e) {
        r2.x(r2.x() + e.delta.x);
        r2.y(r2.y() + e.delta.y);
    });

    //overlay
    var overlay = new amino.Rect().w(300).h(300).fill('#00ff00').x(20).y(20).opacity(0.2).id('overlay');

    overlay.acceptsMouseEvents = false;
    root.add(overlay);

    //scroll
    var scroll = new amino.Rect().w(50).h(200).fill('#0000ff').x(400).y(50).id('scroll');

    root.add(scroll);
    scroll.acceptsScrollEvents = true;

    core.on('scroll', scroll, function (e) {
        scroll.y(scroll.y() + e.position);
    });

    overlay.acceptsKeyboardEvents = true;

    core.on('key.press', overlay, function (e) {
        console.log('key.press event', e.keycode, e.printable, e.char);
    });

});
