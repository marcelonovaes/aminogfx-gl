'use strict';

const amino = require('../../main.js');
const path = require('path');

//func call test (must fail)
//console.log('res: ' + amino.AminoGfx());

//basic tests
const gfx = new amino.AminoGfx();

console.log('instance: ' + gfx);

//screen
console.log('screen: ' + JSON.stringify(gfx.screen));

//default size
console.log('default size: ' + gfx.w() + 'x' + gfx.h());

//fonts
testFont();

//start
gfx.start(function (err) {
    if (err) {
        console.log('Amino error: ' + err.message);
        return;
    }

    console.log('started');

    //runtime
    console.log('runtime: ' + JSON.stringify(gfx.runtime));

    //show position
    console.log('position: ' + this.x() + '/' + this.y());
    //this.x(0).y(0);

    //modify size
    this.w(400);
    this.h(400);
    this.fill('#0000FF');

    //create group
    const g = this.createGroup();

    g.w.bindto(this.w);
    g.h.bindto(this.h);

    this.setRoot(g);

    //add rect
    const r = (this.createRect()).w(100).h(100).fill('#FF0000');

    r.originX(.5).originY(.5).rz(45);

    g.add(r);

    //animation
//    r.b.anim().from(0).to(1).dur(2000).autoreverse(true).loop(1).then(animDone).start();
    r.b.watch((value) => {
        console.log('animation state: ' + value);
    });

    function animDone() {
        console.log('animation done');
    }

    //polygon
    var poly = this.createPolygon().dimension(2).fill('#00FF00');

    poly.geometry([100, 10, 150, 300, 200, 10]);
    g.add(poly);

    //circle
    var circle = this.createCircle().radius(50)
        .fill('#ffcccc').filled(true)
        .x(200).y(200).opacity(0.2);

    g.add(circle);

    //image
    var iv = this.createImageView().x(300).y(300);

    iv.src(path.join(__dirname, '../images/tree.png'));

    // 1) stretch
    iv.size('stretch');

    // 2) resize
    //iv.size('resize');

    // 3) cover
    /*
    iv.w(100).h(50);
    iv.size('cover');
    */

    // 4) contain
    iv.w(100).h(50);
    iv.size('contain');

    g.add(iv);

    //GC tests
    testImages(g);

    //text
    var text = this.createText()
        .text('This is a very long text which is wrapped.\nNew line here.\n  white space.  ')
        .fontSize(80)
        .x(0).y(0)
        .vAlign('top')
        .w(300)
        .h(160)
        .wrap('word')
        .fill('#ffff00');

    //text.text('äöü');

    g.add(text);

    //TODO more cbx
});

function testImages(g) {
    let items = [];
    let w = g.w();
    let h = g.h();
    let img = path.join(__dirname, '../images/tree.png');

    //FIXME 10000: Too many open files in system
    const count = 1000;

    for (let i = 0; i < count; i++) {
        var iv = gfx.createImageView().x(Math.random() * w).y(Math.random() * h);

        iv.src(img);

        items.push(iv);
        g.add(iv);
    }

    setTimeout(() => {
        items.forEach((item) => {
            g.remove(item);
        });
    }, 4000);

    /*
     * References:
     *
     * - ImageView (gets zero -> deallocated)
     * - Group (gets reduced)
     * - AminoImage (temporary, will be freed first -> deallocated)
     * - Texture (-> deallocated last)
     */
}

function testFont() {
    //default font
    amino.fonts.getFont(null, function (err, font) {
        if (err) {
            console.log('could not load font: ' + err.message);
            return;
        }

        console.log('default font: ' + JSON.stringify(font));
        console.log('font metrics: ' + JSON.stringify(font.getFontMetrics()));

        //width
        const startTime = (new Date()).getTime();

        font.calcTextWidth('The quick brown fox jumps over the lazy dog. Pack meinen Kasten mit fünf Dutzend Alkoholkrügen.', function (err, width) {
            if (err) {
                console.log('could not get text width: ' + err.message);
                return;
            }

            console.log('width: ' + width);

            const endTime = (new Date()).getTime();

            console.log(' -> in ' + (endTime - startTime) + ' ms');
        });

    });
}
