// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

tvcm.require('cc.layer_tree_host_impl');
tvcm.require('cc.layer_tree_host_impl_view');
tvcm.require('tracing.importer.trace_event_importer');
tvcm.require('tracing.trace_model');
tvcm.requireRawScript('cc/layer_tree_host_impl_test_data.js');

tvcm.unittest.testSuite('cc.layer_tree_host_impl_view_test', function() {
  test('instantiate', function() {
    var m = new tracing.TraceModel(g_catLTHIEvents);
    var p = tvcm.dictionaryValues(m.processes)[0];

    var instance = p.objects.getAllInstancesNamed('cc::LayerTreeHostImpl')[0];
    var snapshot = instance.snapshots[0];

    var view = new cc.LayerTreeHostImplSnapshotView();
    view.style.width = '900px';
    view.style.height = '400px';
    view.objectSnapshot = snapshot;

    this.addHTMLOutput(view);
  });
});
