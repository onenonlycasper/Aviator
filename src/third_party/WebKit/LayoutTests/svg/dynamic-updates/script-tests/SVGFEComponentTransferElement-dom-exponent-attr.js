// [Name] SVGFEComponentTransferElement-dom-exponent-attr.js
// [Expected rendering result] An image with feComponentTransfer filter - and a series of PASS messages

description("Tests dynamic updates of the 'exponent' attribute of the SVGFEComponentTransferElement object")
createSVGTestCase();

var feRFunc = createSVGElement("feFuncR");
feRFunc.setAttribute("type", "gamma");
feRFunc.setAttribute("exponent", "2");

var feGFunc = createSVGElement("feFuncG");
feGFunc.setAttribute("type", "gamma");
feGFunc.setAttribute("exponent", "2");

var feBFunc = createSVGElement("feFuncB");
feBFunc.setAttribute("type", "gamma");
feBFunc.setAttribute("exponent", "2");

var feAFunc = createSVGElement("feFuncA");
feAFunc.setAttribute("type", "gamma");
feAFunc.setAttribute("exponent", "2");

var feCompnentTransferElement = createSVGElement("feComponentTransfer");
feCompnentTransferElement.appendChild(feRFunc);
feCompnentTransferElement.appendChild(feGFunc);
feCompnentTransferElement.appendChild(feBFunc);
feCompnentTransferElement.appendChild(feAFunc);

var compTranFilter = createSVGElement("filter");
compTranFilter.setAttribute("id", "compTranFilter");
compTranFilter.setAttribute("filterUnits", "objectBoundingBox");
compTranFilter.setAttribute("x", "0%");
compTranFilter.setAttribute("y", "0%");
compTranFilter.setAttribute("width", "100%");
compTranFilter.setAttribute("height", "100%");
compTranFilter.appendChild(feCompnentTransferElement);

var defsElement = createSVGElement("defs");
defsElement.appendChild(compTranFilter);
rootSVGElement.appendChild(defsElement);

var imageElement = createSVGElement("image");
imageElement.setAttributeNS(xlinkNS, "xlink:href", "../W3C-SVG-1.1/resources/struct-image-01.png");
imageElement.setAttribute("width", "400");
imageElement.setAttribute("height", "200");
imageElement.setAttribute("preserveAspectRatio", "none");
imageElement.setAttribute("filter", "url(#compTranFilter)");
rootSVGElement.appendChild(imageElement);

rootSVGElement.setAttribute("width", "400");
rootSVGElement.setAttribute("height", "200");

shouldBeEqualToString("feRFunc.getAttribute('exponent')", "2");
shouldBeEqualToString("feGFunc.getAttribute('exponent')", "2");
shouldBeEqualToString("feBFunc.getAttribute('exponent')", "2");
shouldBeEqualToString("feAFunc.getAttribute('exponent')", "2");

function repaintTest() {
    feRFunc.setAttribute("exponent", "1");
    feGFunc.setAttribute("exponent", "1");
    feBFunc.setAttribute("exponent", "1");
    feAFunc.setAttribute("exponent", "1");

    shouldBeEqualToString("feRFunc.getAttribute('exponent')", "1");
    shouldBeEqualToString("feGFunc.getAttribute('exponent')", "1");
    shouldBeEqualToString("feBFunc.getAttribute('exponent')", "1");
    shouldBeEqualToString("feAFunc.getAttribute('exponent')", "1");
}

var successfullyParsed = true;
