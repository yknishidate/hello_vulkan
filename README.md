# Hello Vulkan 1.3 in 240 lines

Vulkan 1.3でDynamic Rendering拡張機能がコアに昇格しました。
これにより、いままでよりもかなり簡単にVulkanに入門できるようになったと思います。

このリポジトリでは、Dynamic Renderingを使って三角形を描画するまでを書いており、C++コードはわずか240行程度です。
Vulkanでは三角形を出すのに900行くらいは必要と言われているのに比べると非常に短くなっています。

![image](https://user-images.githubusercontent.com/30839669/206858307-041e39d8-d938-4bcb-9ba6-1df7ed06d7f1.png)

## ビルドと実行

```sh
git clone --recursive https://github.com/yknishidate/hello_vulkan
cd hello_vulkan
cmake . -B build
cmake --build build
cd build
Debug\hello_vulkan.exe # for Windows
```

## 入門記事

https://yknishidate.github.io/hello_vulkan
