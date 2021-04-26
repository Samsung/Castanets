# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
from telemetry.page import shared_page_state

from page_sets.rendering import rendering_story
from page_sets.rendering import story_tags


class SimpleCanvasPage(rendering_story.RenderingStory):
  ABSTRACT_STORY = True
  TAGS = [story_tags.SIMPLE_CANVAS]

  def __init__(self,
               page_set,
               shared_page_state_class=shared_page_state.SharedPageState,
               name_suffix='',
               extra_browser_args=None):
    super(SimpleCanvasPage,
          self).__init__(page_set=page_set,
                         shared_page_state_class=shared_page_state_class,
                         name_suffix=name_suffix,
                         extra_browser_args=extra_browser_args)

  def RunNavigateSteps(self, action_runner):
    super(SimpleCanvasPage, self).RunNavigateSteps(action_runner)
    action_runner.WaitForJavaScriptCondition(
        "document.readyState == 'complete'")

  def RunPageInteractions(self, action_runner):
    with action_runner.CreateInteraction('CanvasAnimation'):
      action_runner.Wait(10)


class CanvasToCanvasDrawPage(SimpleCanvasPage):
  BASE_NAME = 'canvas_to_canvas_draw'
  URL = 'file://../simple_canvas/canvas_to_canvas_draw.html'


class DocsPaper(SimpleCanvasPage):
  BASE_NAME = 'docs_paper.html'
  URL = 'file://../simple_canvas/docs_paper.html'


class DocsResume(SimpleCanvasPage):
  BASE_NAME = 'docs_resume.html'
  URL = 'file://../simple_canvas/docs_resume.html'


class DocsTable(SimpleCanvasPage):
  BASE_NAME = 'docs_table.html'
  URL = 'file://../simple_canvas/docs_table.html'


class DynamicCanvasToHWAcceleratedCanvas(SimpleCanvasPage):
  BASE_NAME = 'dynamic_canvas_to_hw_accelerated_canvas.html'
  URL = 'file://../simple_canvas/dynamic_canvas_to_hw_accelerated_canvas.html'


class DynamicWebglToHWAcceleratedCanvas(SimpleCanvasPage):
  BASE_NAME = 'dynamic_webgl_to_hw_accelerated_canvas.html'
  URL = 'file://../simple_canvas/dynamic_webgl_to_hw_accelerated_canvas.html'


class GetImageData(SimpleCanvasPage):
  BASE_NAME = 'get_image_data.html'
  URL = 'file://../simple_canvas/get_image_data.html'


class HWAcceleratedCanvasToSWCanvas(SimpleCanvasPage):
  BASE_NAME = 'hw_accelerated_canvas_to_sw_canvas.html'
  URL = 'file://../simple_canvas/hw_accelerated_canvas_to_sw_canvas.html'


class PutAndCreateImageBitmapFromImageData(SimpleCanvasPage):
  BASE_NAME = 'put_and_create_imagebitmap_from_imagedata'
  URL = 'file://../simple_canvas/put_and_create_imageBitmap_from_imageData.html'


class PutImageData(SimpleCanvasPage):
  BASE_NAME = 'put_image_data.html'
  URL = 'file://../simple_canvas/put_image_data.html'


class StaticCanvasToHWAcceleratedCanvas(SimpleCanvasPage):
  BASE_NAME = 'static_canvas_to_hw_accelerated_canvas.html'
  URL = 'file://../simple_canvas/static_canvas_to_hw_accelerated_canvas.html'


class StaticWebglToHWAcceleratedCanvas(SimpleCanvasPage):
  BASE_NAME = 'static_webgl_to_hw_accelerated_canvas.html'
  URL = 'file://../simple_canvas/static_webgl_to_hw_accelerated_canvas.html'
