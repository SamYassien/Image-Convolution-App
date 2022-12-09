const filters = {
    laplace: [0, 1, 0, 1, -4, 1, 0, 1, 0],
    gaussian: [1, 2, 1, 2, 4, 2, 1, 2, 1],
    box: [1, 1, 1, 1, 1, 1, 1, 1, 1],
    edge: [1, 0, -1, 0, 0, 0, -1, 0, 1],
    emboss: [-1, -1, 0, -1, 0, 1, 0, 1, 1],
    sharpen: [0, -1, 0, -1, 5, -1, 0, -1, 0],
    motion: [1, 0, 0, 0, 1, 0, 0, 0, 1],
};
var input_img = "";

document.getElementById("open_img").addEventListener("click", function () {
    console.log("open_img");
    let input = document.createElement("input");
    input.type = "file";
    input.onchange = (_) => {
        let files = Array.from(input.files);
        console.log(files);
        if (files.length == 0) return;

        input_img = files[0];
        var img_url = URL.createObjectURL(input_img);
        document.getElementById("img").src = img_url;

        const filter = document.getElementById("filter").value;
        if (filter == "orig") {
            document.getElementById("img2").src = img_url;
        } else {
            if (filters[filter] != undefined) {
                let img2 = processImage(input_img, filters[filter]);
                document.getElementById("img2").src = URL.createObjectURL(img2);
            }
        }
    };
    input.click();
});

const filter_selector = document.getElementById("filter");

filter_selector.addEventListener("change", (event) => {
    var filter = event.target.value;
    console.log(filter);

    if (input_img != "") {
        if (filter == "orig") {
            document.getElementById("img2").src = URL.createObjectURL(input_img);
        } else {
            if (filters[filter] != undefined) {
                let img2 = processImage(input_img, filters[filter]);
                document.getElementById("img2").src = URL.createObjectURL(img2);
            }
        }
    }
});

import ConvLib from "./conv_lib_2.js";
const Module = await ConvLib();
// import { Image } from "./image.js";

function processImage(image_path, filter) {
    // read the file
    const imgUrl = document.getElementById("img").src;
    const canvas = document.createElement("canvas");
    const ctx = canvas.getContext("2d");
    const img = new Image();
    img.src = imgUrl;
    canvas.width = img.width;
    canvas.height = img.height;
    ctx.drawImage(img, 0, 0);
    const imageData = ctx.getImageData(0, 0, canvas.width, canvas.height);
    const data = imageData.data;
    // drop the alpha channel
    const data2 = data.filter((_, i) => i % 4 != 3);

    console.log(data2);
    // get data2 into wasm memory
    const dataPtr = Module._malloc(data2.length);
    const dataHeap = new Uint8Array(Module.HEAPU8.buffer, dataPtr, data2.length);
    dataHeap.set(data2);

    let imgObj = new Module.Image(dataHeap.byteOffset, img.width, img.height, 3);
    let new_image = imgObj.convolution_singlethread(filter, 3);
    var array = new Uint8Array(Module.HEAPU8.buffer, new_image.getDataPtr(), new_image.getSize());
    console.log(array);

    const canvas2 = document.createElement("canvas");
    const ctx2 = canvas2.getContext("2d");
    canvas2.width = new_image.getWidth();
    canvas2.height = new_image.getHeight();
    const imageData2 = ctx2.createImageData(canvas2.width, canvas2.height);
    for (let i = 0, j = 0; i < array.length; i += 3, j += 4) {
        imageData2.data[j] = array[i];
        imageData2.data[j + 1] = array[i + 1];
        imageData2.data[j + 2] = array[i + 2];
        imageData2.data[j + 3] = 255;
    }

    ctx2.putImageData(imageData2, 0, 0);
    const dataURL = canvas2.toDataURL("image/jpeg");
    const blobBin = atob(dataURL.split(",")[1]);
    const array2 = [];
    for (let i = 0; i < blobBin.length; i++) {
        array2.push(blobBin.charCodeAt(i));
    }
    const file = new Blob([new Uint8Array(array2)], { type: "image/jpeg" });
    return file;
}

document.getElementById("save_img").addEventListener("click", function () {
    console.log("save_img");
    let img2 = document.getElementById("img2").src;
    let a = document.createElement("a");
    a.href = img2;
    a.download = "image.jpg";
    a.click();
});
