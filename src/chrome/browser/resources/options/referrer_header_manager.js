// Copyright (c) [2013-2015] WhiteHat. All Rights Reserved. WhiteHat contributions included 
// in this file are licensed under the BSD-3-clause license (the "License") included in 
// the WhiteHat-LICENSE file included in the root directory of the distributed source code 
// archive. Unless required by applicable law or agreed to in writing, software distributed
// under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF
// ANY KIND, either express or implied. See the License for the specific language governing 
// permissions and limitations under the License.

cr.define('options.ReferrerHeader', function() {
  /*const InlineEditableItemList = options.InlineEditableItemList;
  const InlineEditableItem = options.InlineEditableItem;
  const ArrayDataModel = cr.ui.ArrayDataModel;*/ 
  
   /** @const */ var InlineEditableItemList = options.InlineEditableItemList;
  /** @const */ var InlineEditableItem = options.InlineEditableItem;
  /** @const */ var ArrayDataModel = cr.ui.ArrayDataModel;

  /** 
   * Creates a new exceptions list item.
   * @param {string} contentType The type of the list.
   * @param {string} mode The browser mode, 'otr' or 'normal'.
   * @param {boolean} enableAskOption Whether to show an 'ask every time'
   *     option in the select.
   * @param {Object} exception A dictionary that contains the data of the
   *     exception.
   * @constructor
   * @extends {options.InlineEditableItem}
   */
  function ExceptionsListItem(contentType, mode, enableAskOption, exception) {
    var el = cr.doc.createElement('div');
    el.mode = mode;
    el.contentType = contentType;
    el.enableAskOption = enableAskOption; 
    el.dataItem = exception;
    el.__proto__ = ExceptionsListItem.prototype;
    el.decorate();

    return el;
  }

  ExceptionsListItem.prototype = {
    __proto__: InlineEditableItem.prototype,

    /**
     * Called when an element is decorated as a list item.
     */
    decorate: function() {
      InlineEditableItem.prototype.decorate.call(this);

      this.isPlaceholder = !this.pattern;
      var patternCell = this.createEditableTextCell(this.pattern);
      patternCell.className = 'exception-pattern';
      patternCell.classList.add('weakrtl');
      this.contentElement.appendChild(patternCell);
      if (this.pattern)
        this.patternLabel = patternCell.querySelector('.static-text');
      var input = patternCell.querySelector('input');

      // TODO(stuartmorgan): Create an createEditableSelectCell abstracting
      // this code.
      // Setting label for display mode. |pattern| will be null for the 'add new
      // exception' row.
      if (this.pattern) {
        var settingLabel = cr.doc.createElement('span');
        settingLabel.textContent = this.settingForDisplay();
        settingLabel.className = 'exception-setting';
        settingLabel.setAttribute('displaymode', 'static');
        this.contentElement.appendChild(settingLabel);
        this.settingLabel = settingLabel;
      }

      // Setting select element for edit mode.
      var select = cr.doc.createElement('select');
      var optionAllow = cr.doc.createElement('option');
      optionAllow.textContent = loadTimeData.getString('allowException');
      optionAllow.value = 'allow';
      select.appendChild(optionAllow);

      if (this.enableAskOption) {
        var optionAsk = cr.doc.createElement('option');
        optionAsk.textContent = loadTimeData.getString('askException');
        optionAsk.value = 'ask';
        select.appendChild(optionAsk); 
      }

      if (this.contentType == 'cookies') {
        var optionSession = cr.doc.createElement('option');
        optionSession.textContent = loadTimeData.getString('sessionException');
        optionSession.value = 'session';
        select.appendChild(optionSession);
      }

      if (this.contentType != 'fullscreen') {
        var optionBlock = cr.doc.createElement('option');
        optionBlock.textContent = loadTimeData.getString('blockException');
        optionBlock.value = 'block';
        select.appendChild(optionBlock);
      }

      this.contentElement.appendChild(select);
      select.className = 'exception-setting';
      if (this.pattern)
        select.setAttribute('displaymode', 'edit');

      // Used to track whether the URL pattern in the input is valid.
      // This will be true if the browser process has informed us that the
      // current text in the input is valid. Changing the text resets this to
      // false, and getting a response from the browser sets it back to true.
      // It starts off as false for empty string (new exceptions) or true for
      // already-existing exceptions (which we assume are valid).
      this.inputValidityKnown = this.pattern;
      // This one tracks the actual validity of the pattern in the input. This
      // starts off as true so as not to annoy the user when he adds a new and
      // empty input.
      this.inputIsValid = true;

      this.input = input;
      this.select = select;

      this.updateEditables();

      // Editing notifications and geolocation is disabled for now.
      if (this.contentType == 'notifications' ||
          this.contentType == 'location') {
        this.editable = false;
      }

      // If the source of the content setting exception is not the user
      // preference, then the content settings exception is managed and the user
      // can't edit it.
      if (this.dataItem.source  &&
          this.dataItem.source != 'preference') {
        this.setAttribute('managedby', this.dataItem.source);
        this.deletable = false;
        this.editable = false;
      }

      var listItem = this;
      // Handle events on the editable nodes.
      input.oninput = function(event) {
        listItem.inputValidityKnown = false;
        chrome.send('checkExceptionPatternValidity',
                    [listItem.contentType, listItem.mode, input.value]);
      };

      // Listen for edit events.
      this.addEventListener('canceledit', this.onEditCancelled_);
      this.addEventListener('commitedit', this.onEditCommitted_);
    },

    isEmbeddingRule: function() {
      return this.dataItem.embeddingOrigin &&
          this.dataItem.embeddingOrigin !== this.dataItem.origin;
    },
    
    
         /**
     * The pattern (e.g., a URL) for the exception.
     *
     * @type {string}
     */
    get pattern() {
 
       if (!this.isEmbeddingRule()) {
        return this.dataItem.origin;
      } else {
        return loadTimeData.getStringF('embeddedOnHost',
                                       this.dataItem.embeddingOrigin);
      }

      return this.dataItem.displayPattern;
    },
    set pattern(pattern) {
      /*if (!this.editable)
        console.error('Tried to change uneditable pattern');*/

      this.dataItem.displayPattern = pattern;
    },

    /**
     * The setting (allow/block) for the exception.
     * @type {string}
     */
    get setting() {
      /*return this.dataItem['setting']; champion */
      return this.dataItem.setting;
    },
    set setting(setting) {
      /*this.dataItem['setting'] = setting; champion */
      this.dataItem.setting = setting;
    },

    /**
     * Gets a human-readable setting string.
     * @type {string}
     */
    settingForDisplay: function() {
      var setting = this.setting;
      if (setting == 'allow')
        return loadTimeData.getString('allowException');
      else if (setting == 'block')
        return loadTimeData.getString('blockException');
      else if (setting == 'ask')
        return loadTimeData.getString('askException');
      else if (setting == 'session')
        return loadTimeData.getString('sessionException');
    },

    /**
     * Update this list item to reflect whether the input is a valid pattern.
     * @param {boolean} valid Whether said pattern is valid in the context of
     *     a content exception setting.
     */
    setPatternValid: function(valid) {
      if (valid || !this.input.value)
        this.input.setCustomValidity('');
      else
        this.input.setCustomValidity(' ');
      this.inputIsValid = valid;
      this.inputValidityKnown = true;
    },

    /**
     * Set the <input> to its original contents. Used when the user quits
     * editing.
     */
    resetInput: function() {
      this.input.value = this.pattern;
    },

    /**
     * Copy the data model values to the editable nodes.
     */
    updateEditables: function() {
      this.resetInput();

      var settingOption =
          this.select.querySelector('[value=\'' + this.setting + '\']');
      if (settingOption)
        settingOption.selected = true;
    },

    /** @inheritDoc */
    get currentInputIsValid() {
      return this.inputValidityKnown && this.inputIsValid;
    },

    /** @inheritDoc */
    get hasBeenEdited() {
      var livePattern = this.input.value;
      var liveSetting = this.select.value;
      return livePattern != this.pattern || liveSetting != this.setting;
    },

    /**
     * Called when committing an edit.
     * @param {Event} e The end event.
     * @private
     */
    onEditCommitted_: function(e) {
      var newPattern = this.input.value;
      var newSetting = this.select.value;

      this.finishEdit(newPattern, newSetting);
    },

    /**
     * Called when cancelling an edit; resets the control states.
     * @param {Event} e The cancel event.
     * @private
     */
    onEditCancelled_: function() {
      this.updateEditables();
      this.setPatternValid(true);
    
    },

    /**
     * Editing is complete; update the model.
     * @param {string} newPattern The pattern that the user entered.
     * @param {string} newSetting The setting the user chose.
     */
    finishEdit: function(newPattern, newSetting) {
      this.patternLabel.textContent = newPattern;
      this.settingLabel.textContent = this.settingForDisplay();
      var oldPattern = this.pattern;
      this.pattern = newPattern;
      this.setting = newSetting;

      // TODO(estade): this will need to be updated if geolocation/notifications
      // become editable.
      if (oldPattern != newPattern) {
        chrome.send('removeException',
                    [this.contentType, this.mode, oldPattern]);
      }
       
      chrome.send('setException',
                  [this.contentType, this.mode, newPattern, newSetting]);
    }
  };

  /**
   * Creates a new list item for the Add New Item row, which doesn't represent
   * an actual entry in the exceptions list but allows the user to add new
   * exceptions.
   * @param {string} contentType The type of the list.
   * @param {string} mode The browser mode, 'otr' or 'normal'.
   * @param {boolean} enableAskOption Whether to show an 'ask every time'
   *     option in the select.
   * @constructor
   * @extends {cr.ui.ExceptionsListItem}
   */ 
  function ExceptionsAddRowListItem(contentType, mode, enableAskOption) {
    var el = cr.doc.createElement('div');
    el.mode = mode;
    el.contentType = contentType;
    el.enableAskOption = enableAskOption;
    el.dataItem = [];
    el.__proto__ = ExceptionsAddRowListItem.prototype;
    el.decorate();

    return el;
  }

  ExceptionsAddRowListItem.prototype = {
    __proto__: ExceptionsListItem.prototype,

    decorate: function() {
      ExceptionsListItem.prototype.decorate.call(this);

      this.input.placeholder = loadTimeData.getString('addNewExceptionInstructions');

      // Do we always want a default of allow?
      this.setting = 'allow';
    },

    /**
     * Clear the <input> and let the placeholder text show again.
     */
    resetInput: function() {
      this.input.value = '';
    },

    /** @inheritDoc */
    get hasBeenEdited() {
      return this.input.value != '';
    },

    /**
     * Editing is complete; update the model. As long as the pattern isn't
     * empty, we'll just add it.
     * @param {string} newPattern The pattern that the user entered.
     * @param {string} newSetting The setting the user chose.
     */
    finishEdit: function(newPattern, newSetting) {
      this.resetInput();
      chrome.send('setException',
                  [this.contentType, this.mode, newPattern, newSetting]);
    },
  };

  /**
   * Creates a new exceptions list.
   * @constructor
   * @extends {cr.ui.List}
   */
  var ExceptionsList = cr.ui.define('list');

  ExceptionsList.prototype = {
    __proto__: InlineEditableItemList.prototype,

    /**
     * Called when an element is decorated as a list.
     */
    decorate: function() {
      InlineEditableItemList.prototype.decorate.call(this);

      this.classList.add('settings-list');

      for (var parentNode = this.parentNode; parentNode;
           parentNode = parentNode.parentNode) {
        if (parentNode.hasAttribute('contentType')) {
          this.contentType = parentNode.getAttribute('contentType');
          break;
        }
      }

      this.mode = this.getAttribute('mode');

      var exceptionList = this;

      // Whether the exceptions in this list allow an 'Ask every time' option.
      this.enableAskOption = (this.contentType == 'plugins' &&
                              loadTimeData.getString('enable_click_to_play'));

      this.autoExpands = true;
      this.reset();
    },

    /**
     * Creates an item to go in the list.
     * @param {Object} entry The element from the data model for this row.
     */
    createItem: function(entry) {
      if (entry) {
        return new ExceptionsListItem(this.contentType,
                                      this.mode,
                                      this.enableAskOption,
                                      entry);
      } else {
        var addRowItem = new ExceptionsAddRowListItem(this.contentType,
                                                      this.mode,
                                                      this.enableAskOption);
        addRowItem.deletable = false;
        return addRowItem;
      }
    },

    /**
     * Sets the exceptions in the js model.
     * @param {Object} entries A list of dictionaries of values, each dictionary
     *     represents an exception.
     */
    setExceptions: function(entries) {
      var deleteCount = this.dataModel.length;

      if (this.isEditable()) {
        // We don't want to remove the Add New Exception row.
        deleteCount = deleteCount - 1;
      }

      var args = [0, deleteCount];
      args.push.apply(args, entries);
      this.dataModel.splice.apply(this.dataModel, args);
    },

    /**
     * The browser has finished checking a pattern for validity. Update the
     * list item to reflect this.
     * @param {string} pattern The pattern.
     * @param {bool} valid Whether said pattern is valid in the context of
     *     a content exception setting.
     */
    patternValidityCheckComplete: function(pattern, valid) {
      var listItems = this.items;
      for (var i = 0; i < listItems.length; i++) {
        var listItem = listItems[i];
        // Don't do anything for messages for the item if it is not the intended
        // recipient, or if the response is stale (i.e. the input value has
        // changed since we sent the request to analyze it).
        if (pattern == listItem.input.value)
          listItem.setPatternValid(valid);
      }
    },

    /**
     * Returns whether the rows are editable in this list.
     */
    isEditable: function() {
      // Editing notifications and geolocation is disabled for now.
      return !(this.contentType == 'notifications' ||
               this.contentType == 'location' ||
               this.contentType == 'fullscreen');
    },

    /**
     * Removes all exceptions from the js model.
     */
    reset: function() {
      if (this.isEditable()) {
        // The null creates the Add New Exception row.
        this.dataModel = new ArrayDataModel([null]);
      } else {
        this.dataModel = new ArrayDataModel([]);
      }
    },

    /** @inheritDoc */
    deleteItemAtIndex: function(index) {
      var listItem = this.getListItemByIndex(index);
      /*if (listItem.undeletable)*/ /*** champion*****/
       if (!listItem.deletable)
        return;

      var dataItem = listItem.dataItem;
      var args = [listItem.contentType];
      /*(if (listItem.contentType == 'location')
        args.push(dataItem['origin'], dataItem['embeddingOrigin']);
      else if (listItem.contentType == 'notifications')
        args.push(dataItem['origin'], dataItem['setting']);
      else
        args.push(listItem.mode, listItem.pattern);*/ /****champion***/
      if (listItem.contentType == 'notifications')
        args.push(dataItem.origin, dataItem.setting);
      else
        args.push(listItem.mode, dataItem.origin, dataItem.embeddingOrigin);

      chrome.send('removeException', args);
    },
  };
  
  var OptionsPage = options.OptionsPage;
  
  /**
   * Encapsulated handling of content settings list subpage.
   * @constructor
   */
  function ReferrerHeaderExceptionsArea() {
    OptionsPage.call(this, 'ManageReferrerHeader',
                     loadTimeData.getString('referrerHeaderPageTabTitle'),
                     'referrer-header-manager');
  }

  cr.addSingletonGetter(ReferrerHeaderExceptionsArea);

  ReferrerHeaderExceptionsArea.prototype = {
    __proto__: OptionsPage.prototype,

    initializePage: function() {
      OptionsPage.prototype.initializePage.call(this); 
	  /*$('export').onclick = function(event) {
			chrome.send('selectDownloadLocation');
	  };*/
	  $('closeReferrerHeaderManager').onclick = function(event) {
			OptionsPage.closeOverlay();
	  }; 
	  $('helpIconImport').onclick = function(event) {
			OptionsPage.navigateToPage('referrerImportHelpHeader');        
	  };
	  $('import').onchange = function(event) {
		var files = event.target.files; // FileList object
		// Loop through the FileList
        for (var i = 0, f; f = files[i]; i++) {

          var reader = new FileReader();

          // Closure to capture the file information.
          reader.onload = (function(theFile) {
            return function(e) {
              // Print the contents of the file
              var spans = e.target.result;
			  var pattern;
			  var arrrr=spans.split(",");
			  for(i=0;i<arrrr.length;i++){
				var dom=arrrr[i].split(";");
				if(dom[1]=="1")
					pattern="allow";
				else
					pattern="block";
				chrome.send('setException',["referrer", "import", dom[0], pattern]);
			  }
            };
          })(f);

          // Read in the file
          //reader.readAsDataText(f,UTF-8);
          reader.readAsText(f,"UTF-8");
        }
		document.getElementById('import').value="";
	  };
	  var exceptionsLists = this.pageDiv.querySelectorAll('list');
      for (var i = 0; i < exceptionsLists.length; i++) {
        options.ReferrerHeader.ExceptionsList.decorate(exceptionsLists[i]);
      }
    },
	
	showList: function() {
      var header = this.pageDiv.querySelector('h1');
      header.textContent = 'Referrer Header';

      var divs = this.pageDiv.querySelectorAll('div[contentType]');
      for (var i = 0; i < divs.length; i++) {
        if (divs[i].getAttribute('contentType') == 'referrer')
          divs[i].hidden = false;
        else
          divs[i].hidden = true;
      }
    },
	

    /**
     * Called after the page has been shown. Show the content type for the
     * location's hash.
     */
    didShowPage: function() {
      this.showList();
    },
};

	


return {
    ExceptionsListItem: ExceptionsListItem,
    ExceptionsAddRowListItem: ExceptionsAddRowListItem,
    ExceptionsList: ExceptionsList,
    ReferrerHeaderExceptionsArea: ReferrerHeaderExceptionsArea
  };
});

