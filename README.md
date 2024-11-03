# Polygonal Watercolour

A simple painting application implementing **Painting with Polygons: A Procedural Watercolor Engine**, a paper by Stephen DiVerdi, Aravind Krishnaswamy, Radomír Měch, and Daichi Ito.

<img src='demo.png'>

This application lets you paint with "super-wet" watercolour using the mouse. Hold down the left mouse button to place a stroke, or the right mouse button to only add water to the canvas. Use the scroll wheel to zoom in and the middle mouse button to pan the canvas.

Paint will "advect" under its own motion for some time after it is added before becoming fixed. Fixed strokes can still be rewetted to flow again by adding more water to the canvas, but after a while they will dry and become permanent. You can undo strokes which still haven't dried yet using the menu or the Ctrl+Z/Ctrl+Y hotkeys.

For an explanation of the algorithm and its characteristics, see:

Stephen DiVerdi, Aravind Krishnaswamy, Radomír Měch, and Daichi Ito. Painting with polygons: A procedural watercolor engine. _IEEE Transactions on Visualization and Computer Graphics_, 19:723–735, 2013. 1, 7, 8
