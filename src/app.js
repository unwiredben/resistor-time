var PEBBLE_COLORS = {
    Black:                 parseInt("11000000", 2),
    OxfordBlue:            parseInt("11000001", 2),
    DukeBlue:              parseInt("11000010", 2),
    Blue:                  parseInt("11000011", 2),
    DarkGreen:             parseInt("11000100", 2),
    MidnightGreen:         parseInt("11000101", 2),
    CobaltBlue:            parseInt("11000110", 2),
    BlueMoon:              parseInt("11000111", 2),
    IslamicGreen:          parseInt("11001000", 2),
    JaegerGreen:           parseInt("11001001", 2),
    TiffanyBlue:           parseInt("11001010", 2),
    VividCerulean:         parseInt("11001011", 2),
    Green:                 parseInt("11001100", 2),
    Malachite:             parseInt("11001101", 2),
    MediumSpringGreen:     parseInt("11001110", 2),
    Cyan:                  parseInt("11001111", 2),
    BulgarianRose:         parseInt("11010000", 2),
    ImperialPurple:        parseInt("11010001", 2),
    Indigo:                parseInt("11010010", 2),
    ElectricUltramarine:   parseInt("11010011", 2),
    ArmyGreen:             parseInt("11010100", 2),
    DarkGray:              parseInt("11010101", 2),
    Liberty:               parseInt("11010110", 2),
    VeryLightBlue:         parseInt("11010111", 2),
    KellyGreen:            parseInt("11011000", 2),
    MayGreen:              parseInt("11011001", 2),
    CadetBlue:             parseInt("11011010", 2),
    PictonBlue:            parseInt("11011011", 2),
    BrightGreen:           parseInt("11011100", 2),
    ScreaminGreen:         parseInt("11011101", 2),
    MediumAquamarine:      parseInt("11011110", 2),
    ElectricBlue:          parseInt("11011111", 2),
    DarkCandyAppleRed:     parseInt("11100000", 2),
    JazzberryJam:          parseInt("11100001", 2),
    Purple:                parseInt("11100010", 2),
    VividViolet:           parseInt("11100011", 2),
    WindsorTan:            parseInt("11100100", 2),
    RoseVale:              parseInt("11100101", 2),
    Purpureus:             parseInt("11100110", 2),
    LavenderIndigo:        parseInt("11100111", 2),
    Limerick:              parseInt("11101000", 2),
    Brass:                 parseInt("11101001", 2),
    LightGray:             parseInt("11101010", 2),
    BabyBlueEyes:          parseInt("11101011", 2),
    SpringBud:             parseInt("11101100", 2),
    Inchworm:              parseInt("11101101", 2),
    MintGreen:             parseInt("11101110", 2),
    Celeste:               parseInt("11101111", 2),
    Red:                   parseInt("11110000", 2),
    Folly:                 parseInt("11110001", 2),
    FashionMagenta:        parseInt("11110010", 2),
    Magenta:               parseInt("11110011", 2),
    Orange:                parseInt("11110100", 2),
    SunsetOrange:          parseInt("11110101", 2),
    BrilliantRose:         parseInt("11110110", 2),
    ShockingPink:          parseInt("11110111", 2),
    ChromeYellow:          parseInt("11111000", 2),
    Rajah:                 parseInt("11111001", 2),
    Melon:                 parseInt("11111010", 2),
    RichBrilliantLavender: parseInt("11111011", 2),
    Yellow:                parseInt("11111100", 2),
    Icterine:              parseInt("11111101", 2),
    PastelYellow:          parseInt("11111110", 2),
    White:                 parseInt("11111111", 2)
};

var sendSuccess = function(e) {
    console.log("sendSuccess, e: " + JSON.stringify(e));
    //console.log(
    //    'Successfully delivered message with transactionId=' +
    //    e.data.transactionId);
};

var sendFailure = function(e) {
    console.log("sendFailure, e: " + JSON.stringify(e));
    //console.log(
    //    'Unable to deliver message with transactionId=' +
    //    e.data.transactionId +
    //    ' Error is: ' + e.error.message);
};

Pebble.addEventListener("ready", function(e) {
});

Pebble.addEventListener("showConfiguration", function(e) {
    var color = localStorage.getItem("color");
    Pebble.openURL(
        'http://www.combee.net/resistor-time/config.html' +
        (color ? "?color=" + color : ""));
});

Pebble.addEventListener("webviewclosed", function(e) {
    var msg = {};
    var color = e.response;
    switch (color) {
        case "white":
            msg.backgroundColor = PEBBLE_COLORS.White;
            msg.silkScreenColor = PEBBLE_COLORS.Black;
            break;
        case "black":
            msg.backgroundColor = PEBBLE_COLORS.Black;
            msg.silkScreenColor = PEBBLE_COLORS.White;
            break;
        default:
            color = "green";
            /* falls through */
        case "green":
            msg.backgroundColor = PEBBLE_COLORS.KellyGreen;
            msg.silkScreenColor = PEBBLE_COLORS.White;
            break;
    }
    localStorage.setItem("color", color);
    Pebble.sendAppMessage(msg, sendSuccess, sendFailure);
});