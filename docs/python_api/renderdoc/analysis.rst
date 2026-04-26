API Reference: Replay Analysis
==============================

This is the API reference for the functions, classes, and enums in the ``renderdoc`` module which represents the underlying interface that the UI is built on top of. For more high-level information and instructions on using the python API, see :doc:`../index`.

.. contents:: Sections
   :local:

.. currentmodule:: renderdoc

Frame and Actions
-----------------

.. autoclass:: FrameDescription
  :members:

.. autoclass:: ActionDescription
  :members:

.. autoclass:: ActionFlags
  :members:

.. autoclass:: APIEvent
  :members:

Debug Messages
--------------

.. autoclass:: DebugMessage
  :members:

.. autoclass:: MessageCategory
  :members:

.. autoclass:: MessageSeverity
  :members:

.. autoclass:: MessageSource
  :members:

Resource Usage
--------------

.. autoclass:: EventUsage
  :members:

.. autoclass:: ResourceUsage
  :members:

.. autofunction:: renderdoc.ResUsage
.. autofunction:: renderdoc.RWResUsage
.. autofunction:: renderdoc.CBUsage

.. py:function:: ShaderUsage(stage)

   Calculate the ``ResourceUsage`` value for a shader object use at a given shader stage.

   :param ShaderStage stage: The shader stage.
   :return: The value for shader object usage at a given shader stage.
   :rtype: ResourceUsage

Texture Saving
--------------

.. autoclass:: TextureSave
  :members:

.. autoclass:: FileType
  :members:

.. autoclass:: AlphaMapping
  :members:

.. autoclass:: TextureComponentMapping
  :members:

.. autoclass:: TextureSampleMapping
  :members:

.. autoclass:: TextureSliceMapping
  :members:

Pixel History
-------------

.. autoclass:: PixelModification
  :members:

.. autoclass:: ModificationValue
  :members:

.. autoclass:: PixelValue
  :members:

Shader Debuging
---------------

.. autoclass:: DebugPixelInputs
  :members:
