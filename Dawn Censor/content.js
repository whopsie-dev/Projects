// There is no license, change this however you want. This was purely done because of my extreme hatred towards
// Dawn launcher (Sorry for sensitive words).

const targetWord = /dawn/gi; 
const censoredWord = "D*wn";

function censorText(node) {
    if (node.nodeType === 3) {
        const text = node.nodeValue;
        const replacedText = text.replace(targetWord, censoredWord);
        if (replacedText !== text) {
            node.nodeValue = replacedText;
        }
    } 
    else if (node.nodeType === 1 && node.nodeName !== 'SCRIPT' && node.nodeName !== 'STYLE') {
        for (let i = 0; i < node.childNodes.length; i++) {
            censorText(node.childNodes[i]);
        }
    }
}

censorText(document.body);

const observer = new MutationObserver((mutations) => {
    mutations.forEach((mutation) => {
        mutation.addedNodes.forEach((addedNode) => {
            censorText(addedNode);
        });
    });
});

if (document.body) {
    observer.observe(document.body, {
        childList: true,
        subtree: true
    });
}