/**
 * Appcelerator Titanium Mobile
 * Copyright (c) 2011-Present by Appcelerator, Inc. All Rights Reserved.
 * Licensed under the terms of the Apache Public License
 * Please see the LICENSE included with this distribution for details.
 */
'use strict';

/**
 * main bootstrapping function
 * @param  {object} kroll [description]
 * @return {void}       [description]
 */
(kroll => { // eslint-disable-line no-unused-expressions
	const evaluate = kroll.binding('evals');
	var global = this;

	// Works identical to Object.hasOwnProperty, except
	// also works if the given object does not have the method
	// on its prototype or it has been masked.
	function hasOwnProperty(object, property) {
		return Object.hasOwnProperty.call(object, property);
	}

	kroll.extend = function (thisObject, otherObject) {
		if (!otherObject) {
			// extend with what?!  denied!
			return;
		}

		for (var name in otherObject) {
			if (hasOwnProperty(otherObject, name)) {
				thisObject[name] = otherObject[name];
			}
		}

		return thisObject;
	};

	function startup() {
		startup.globalVariables();
		startup.runMain();
	}

	// Used just to differentiate scope vars on java side by
	// using a unique constructor name
	function ScopeVars(vars) {
		if (!vars) {
			return this;
		}

		var keys = Object.keys(vars);
		var length = keys.length;

		for (var i = 0; i < length; ++i) {
			var key = keys[i];
			this[key] = vars[key];
		}
	}

	startup.globalVariables = function () {
		global.kroll = kroll;
		kroll.ScopeVars = ScopeVars;
		kroll.NativeModule = NativeModule; // So external module bootstrap.js can call NativeModule.require directly.

		NativeModule.require('events');
		global.Ti = global.Titanium = NativeModule.require('titanium');
		global.Module = NativeModule.require('module');
	};

	startup.runMain = function () {};

	function NativeModule(id) {
		this.filename = id + '.js';
		this.id = id;
		this.exports = {};
		this.loaded = false;
	}

	NativeModule._source = kroll.binding('natives');
	NativeModule._cache = {};

	NativeModule.require = function (id) {
		if (id === 'native_module') {
			return NativeModule;
		}

		var cached = NativeModule.getCached(id);
		if (cached) {
			return cached.exports;
		}

		if (!NativeModule.exists(id)) {
			throw new Error('No such native module ' + id);
		}

		var nativeModule = new NativeModule(id);

		nativeModule.compile();
		nativeModule.cache();

		return nativeModule.exports;
	};

	NativeModule.getCached = function (id) {
		return NativeModule._cache[id];
	};

	NativeModule.exists = function (id) {
		return (id in NativeModule._source);
	};

	NativeModule.getSource = function (id) {
		return NativeModule._source[id];
	};

	NativeModule.prototype.compile = function () {
		var source = NativeModule.getSource(this.id);

		// All native modules have their filename prefixed with ti:/
		var filename = 'ti:/' + this.filename;

		const result = evaluate.runAsModule(source, filename, {
			exports: this.exports,
			require: NativeModule.require,
			module: this,
			__filename: filename,
			__dirname: null,
			Ti: global.Ti,
			Titanium: global.Ti,
			global,
			kroll
		});
		if (result) {
			kroll.extend(this.exports, result);
		}

		this.loaded = true;
	};

	NativeModule.prototype.cache = function () {
		NativeModule._cache[this.id] = this;
	};

	startup();
});
