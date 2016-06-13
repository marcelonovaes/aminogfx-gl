'use strict';

var amino = require('./src/amino.js');
var input = require('./src/aminoinput.js');

exports.input = input;

/**
 * Core object.
 */
var Core = function () {
    this._core = null;
    this.anims = [];
    this.root = null;
    this.native = null;
    this.DPIScale = 1.0;

    var self = this;

    /**
     * Get native API access.
     *
     * TODO replace by OOP handling
     */
    this.getNative = function () {
    	return this.native;
    };

    /**
     * Initialize.
     */
    this.init = function() {
        this.native.init(this);

        this.native.setEventCallback(function (e) {
            if (!e || e == null) {
                console.log("ERROR. null event");
                return;
            }

            //console.log("event type is", e.type);

            e.time = new Date().getTime();

            //DPI scaling
            if (e.x) {
                e.x /= self.DPIScale;
            }

            if (e.y) {
                e.y /= self.DPIScale;
            }

            if (e.type == "windowsize") {
                e.width /= self.DPIScale;
                e.height /= self.DPIScale;
            }

            input.processEvent(self,e);
        });
    };

    /**
     * Register a font.
     */
	this.registerFont = function (args) {
	    this.getNative().registerFont(args);
	};

    /**
     * Windows was resized.
     */
    this.handleWindowSizeEvent = function (evt) {
        //FIXME
        console.log("doing nothing with the resize");
    };

    /**
     * Start.
     */
    this.start = function () {
        //send a final window size event to make sure everything is lined up correctly
        var size = this.native.getWindowSize();

        this.stage.width = size.w;
        this.stage.height = size.h;

        input.processEvent(this, {
            type: "windowsize",
            width: size.w,
            height: size.h
        });

        if (!this.root) {
            throw new Error("ERROR. No root set on stage");
        }

        this.native.startEventLoop();
    };

    /**
     * Create the stage.
     */
    this.createStage = function (w, h) {
        this.native.createWindow(this, w, h);
        this.stage = new Stage(this).w(w).h(h);

        return this.stage;
    };

    /**
     * Get a font by name.
     */
    this.getFont = function (name) {
        return this.native.getFont(name);
    };

    /**
     * Set the root node.
     */
    this.setRoot = function (node) {
        this.native.setRoot(node.handle);
        this.root = node;
    };

    /**
     * Find a node at a certain position.
     */
    this.findNodesAtXY = function (pt) {
        return findNodesAtXY_helper(this.root, pt, null, '');
    };

    /**
     * Find a node at a certain position with a filter callback.
     */
    this.findNodesAtXYFiltered = function (pt, filter) {
        return findNodesAtXY_helper(this.root, pt, filter, '');
    };

    function findNodesAtXY_helper(root, pt, filter, tab) {
        //verify
        if (!root) {
            return [];
        }

        if (!root.visible()) {
            return [];
        }

        //console.log(tab + "   xy",pt.x,pt.y, root.id());

        var tpt = pt.minus(root.x(), root.y()).divide(root.sx(), root.sy());

        //handle children first, then the parent/root
        var res = [];

        if (filter) {
            if (!filter(root)) {
                return res;
            }
        }

        if (root.children && root.children.length && root.children.length > 0) {
            for (var i = root.children.length - 1; i >= 0; i--) {
                var node = root.children[i];
                var found = findNodesAtXY_helper(node, tpt, filter, tab + '  ');

                res = res.concat(found);
            }
        }

        if (root.contains && root.contains(tpt)) {
            res = res.concat([root]);
        }

        return res;
    }

    /**
     * Find a node at a certain position.
     */
    this.findNodeAtXY = function (x, y) {
        return this.findNodeAtXY_helper(this.root, x, y, '');
    };

    this.findNodeAtXY_helper = function (root, x, y, tab) {
        if (!root) {
            return null;
        }

        if (!root.visible()) {
            return null;
        }

        var tx = x - root.x();
        var ty = y - root.y();

        tx /= root.sx();
        ty /= root.sy();

        //console.log(tab + "   xy="+tx+","+ty);

        if (root.cliprect && root.cliprect() == 1) {
            if (tx < 0) {
                return false;
            }

            if (tx > root.w()) {
                return false;
            }

            if (ty < 0) {
                return false;
            }

            if (ty > root.h()) {
                return false;
            }
        }

        if (root.children) {
            //console.log(tab+"children = ",root.children.length);

            for (var i = root.children.length - 1; i >= 0; i--) {
                var node = root.children[i];
                var found = this.findNodeAtXY_helper(node, tx, ty, tab+ '  ');

                if (found) {
                	return found;
            	}
            }
        }

        //console.log(tab+"contains " + tx+' '+ty);

        if (root.contains && root.contains(tx, ty)) {
            //console.log(tab,"inside!",root.getId());

           return root;
        }

        return null;
    };

    function calcGlobalToLocalTransform(node) {
        if (node.parent) {
            var trans = calcGlobalToLocalTransform(node.parent);

            if (node.getScalex() != 1) {
                trans.x /= node.sx();
                trans.y /= node.sy();
            }

            trans.x -= node.x();
            trans.y -= node.y();

            return trans;
        }

        return {
            x: -node.x(),
            y: -node.y()
        };
    }

    this.globalToLocal = function (pt, node) {
        return this.globalToLocal_helper(pt,node);
    };

    this.globalToLocal_helper = function (pt, node) {
    	if (node.parent) {
    		pt =  this.globalToLocal_helper(pt,node.parent);
    	}

        return exports.input.makePoint(
            (pt.x - node.x()) / node.sx(),
            (pt.y - node.y()) / node.sy()
        );
    };

    this.localToGlobal = function (pt, node) {
        pt = {
            x: pt.x + node.x() * node.sx(),
            y: pt.y + node.y() * node.sx()
        };

        if (node.parent) {
            return this.localToGlobal(pt, node.parent);
        } else {
            return pt;
        }
    };

    this.on = function (name, target, listener) {
        exports.input.on(name, target, listener);
    };
};

/**
 * Set the global core instance.
 */
Core.setCore = function (core) {
	Core._core = core;
};

/**
 * Get the global core instance.
 */
Core.getCore = function () {
	return Core._core;
};

exports.Core = Core;

//amino binding
amino.getCore = function() {
	return Core._core;
};

/**
 * Stage object.
 */
function Stage (core) {
	this.core = core;

    amino.makeProps(this, {
        x: 0,
        y: 0,
        w: 100,
        h: 100,
        opacity: 1.0,
        transparent: false,
        smooth: true,
        fill: "#000000"
    });

    //FIXME stage size cannot be changed -> watch w/h
    //TODO are x and y used?

    var self = this;

	/**
     * @func on(name,node,cb) sets a callback for events matching the specified name on the
	specified node. Use null for the node to match global events.
     */
	this.on = function (name, node, cb) {
		core.on(name, node, cb);
	};
	/**
     * @func getRoot returns the root node of this stage.
     */
	this.getRoot = function () {
		return core.root;
	};

	/**
     * @func set the root node of this stage.
     */
	this.setRoot = function (root) {
		core.setRoot(root);

		return this;
	};

	/**
     * @func find(id) searches the stage's node tree for a node with the requested ID. Returns null if no node is found.
     */
    this.find = function (id) {
        return this.findNodeById_helper(id, this.getRoot());
    };

    this.findNodeById_helper = function (id, node) {
        if (node.id && node.id == id) {
            return node;
        }

        if (node.isParent && node.isParent()) {
            for (var i = 0; i < node.getChildCount(); i++) {
                var ret = this.findNodeById_helper(id, node.getChild(i));

                if (ret != null) {
                    return ret;
                }
            }
        }

        return null;
    };

    core.on('windowsize', this, function (e) {
        var root = self.getRoot();

        if (root.setW) {
            root.setW(self.getW());
        }

        if (root.setH) {
            root.setH(self.getH());
        }
    });
}

exports.makeProps = amino.makeProps;

exports.Rect = amino.Rect;
exports.Group = amino.Group;
exports.Text = amino.Text;
exports.Polygon = amino.Polygon;
exports.Circle = amino.Circle;
exports.ImageView = amino.ImageView;
exports.ParseRGBString = amino.primitives.ParseRGBString;
