/* global require */
/* exported clay */

var Clay = require('pebble-clay');
var clayConfig = [
    {
        "type": "heading",
        "defaultValue": "Resistor Time"
    },
    {
        "type": "radiogroup",
        "messageKey": "resType",
        "label": "Resistor type",
        "defaultValue": "0",
        "options": [
            { 
                "label": "Through Hole", 
                "value": "0"
            },
            { 
                "label": "Surface Mount", 
                "value": "1"
            },
            { 
                "label": "NYC Resistor", 
                "value": "2"
            }
        ]
    },
    {
        "type": "radiogroup",
        "messageKey": "choice",
        "label": "Silkscreen color",
        "defaultValue": "white-on-green",
        "options": [
            { 
                "label": "Green with white text", 
                "value": "white-on-green"
            },
            { 
                "label": "Black with white text", 
                "value": "white-on-black"
            },
            { 
                "label": "White with black text", 
                "value": "black-on-white"
            },
            { 
                "label": "OSH Park", 
                "value": "white-on-purple"
            },
            {
                "label": "Custom",
                "value": "custom"
            }
        ]
    },
    {
        "type": "color",
        "messageKey": "bgColor",
        "defaultValue": "55AA00",
        "label": "Background Color",
        "sunlight": true
    },
    {
        "type": "color",
        "messageKey": "fgColor",
        "defaultValue": "FFFFFF",
        "label": "Silkscreen Color",
        "sunlight": true
    },
    {
        "type": "radiogroup",
        "messageKey": "vibeOnBT",
        "label": "Vibrate",
        "defaultValue": "0",
        "options": [
            { 
                "label": "Never", 
                "value": "0"
            },
            { 
                "label": "When phone disconnects", 
                "value": "1"
            },
            { 
                "label": "When phone connects or disconnects", 
                "value": "2"
            }
        ]
    },
    {
        "type": "submit",
        "defaultValue": "Save"
    }
];

function clayCustom() {
    var clayConfig = this;

    function showCustomSelectors() {
        var bgColor = clayConfig.getItemByMessageKey("bgColor");
        var fgColor = clayConfig.getItemByMessageKey("fgColor");
        if (this.get() == "custom") {
            bgColor.show();
            fgColor.show();
        }
        else {
            bgColor.hide();
            fgColor.hide();
        }
    }
    
    clayConfig.on(clayConfig.EVENTS.AFTER_BUILD, function() {
        var colorChoice = clayConfig.getItemByMessageKey('choice');
        showCustomSelectors.call(colorChoice);
        colorChoice.on('change', showCustomSelectors);
    });
}

var clay = new Clay(clayConfig, clayCustom, { autoHandleEvents: false });

Pebble.addEventListener('showConfiguration', function(e) {
    Pebble.openURL(clay.generateUrl());
});

Pebble.addEventListener('webviewclosed', function(e) {
    if (e && !e.response) {
        return;
    }

    // Get the keys and values from each config item
    var dict = clay.getSettings(e.response, false);
    
    console.log("dict: " + JSON.stringify(dict));
    
    var msg = { 
        VIBE_ON_BT: parseInt(dict.vibeOnBT.value),
        SILK_COLOR: 0xFFFFFF, // White
        BG_COLOR:   0x55AA00, // KellyGreen
        RESISTOR_TYPE: parseInt(dict.resType.value)
    };

    // "white-on-green" is default
    if (dict.choice.value == "black-on-white") {
        msg.SILK_COLOR = 0x000000; // Black;
        msg.BG_COLOR =   0xFFFFFF; // White
    } else if (dict.choice.value == "white-on-black") {
        msg.SILK_COLOR = 0xFFFFFF; // White
        msg.BG_COLOR =   0x000000; // Black
    } else if (dict.choice.value == "white-on-purple") {
        msg.SILK_COLOR = 0xFFFFFF; // White
        msg.BG_COLOR =   0x550055; // Imperial Purple
    }
    else if (dict.choice.value == "custom") {
        msg.SILK_COLOR = dict.fgColor.value;
        msg.BG_COLOR = dict.bgColor.value;
    }
    
    console.log("msg: " + JSON.stringify(msg));

    // Send settings values to watch side
    Pebble.sendAppMessage(msg, function(e) {
        console.log('Sent config data to Pebble');
    }, function(e) {
        console.log('Failed to send config data!');
        console.log(JSON.stringify(e));
    });
});