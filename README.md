https://vulkan-tutorial.com/

VS 2022, cpp20

- `VK_KHR_surface`: WSI (Window System Integration) extension. Connects between Vulkan & the windows
system. Exposes `VkSurfaceKHR`, which represents an abstract type of surface to present rendered
images to.


- `glfwGetRequiredInstanceExtensions` returns other WSI extensions

- swap chain: a queue of images that are waiting to be presented to the screen. General purpose is
  to sync the presentation of the images w/ the refresh rate of the screen.
