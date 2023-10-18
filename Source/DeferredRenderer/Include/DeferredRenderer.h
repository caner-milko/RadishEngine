#pragma once

#include "Game.h"
#include <Window.h>

#include <DirectXMath.h>
#include "Common.h"
#include "D3D12/D3D12Common.h"

namespace dfr
{

class DeferredRenderer : public Game
{
public:
	using super = Game;

	DeferredRenderer(const std::wstring& name, int width, int height, bool vSync = false);
	/**
	 *  Load content required for the demo.
	 */
	virtual bool LoadContent() override;

	/**
	 *  Unload demo specific content that was loaded in LoadContent.
	 */
	virtual void UnloadContent() override;
protected:
	/**
	 *  Update the game logic.
	 */
	virtual void OnUpdate(UpdateEventArgs& e) override;

	/**
	 *  Render stuff.
	 */
	virtual void OnRender(RenderEventArgs& e) override;

	/**
	 * Invoked by the registered window when a key is pressed
	 * while the window has focus.
	 */
	virtual void OnKeyPressed(KeyEventArgs& e) override;

	/**
	 * Invoked when the mouse wheel is scrolled while the registered window has focus.
	 */
	virtual void OnMouseWheel(MouseWheelEventArgs& e) override;


	virtual void OnResize(ResizeEventArgs& e) override; 

private:
	// Helper functions
	// Transition a resource
	void TransitionResource(ComPtr<ID3D12GraphicsCommandList2> commandList,
		ComPtr<ID3D12Resource> resource,
		D3D12_RESOURCE_STATES beforeState, D3D12_RESOURCE_STATES afterState);

	// Clear a render target view.
	void ClearRTV(ComPtr<ID3D12GraphicsCommandList2> commandList,
		D3D12_CPU_DESCRIPTOR_HANDLE rtv, FLOAT* clearColor);

	// Clear the depth of a depth-stencil view.
	void ClearDepth(ComPtr<ID3D12GraphicsCommandList2> commandList,
		D3D12_CPU_DESCRIPTOR_HANDLE dsv, FLOAT depth = 1.0f );

	// Create a GPU buffer.
	void UpdateBufferResource(ComPtr<ID3D12GraphicsCommandList2> commandList,
		ID3D12Resource** pDestinationResource, ID3D12Resource** pIntermediateResource,
		size_t numElements, size_t elementSize, const void* bufferData, 
		D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE );

	// Resize the depth buffer to match the size of the client area.
	void ResizeDepthBuffer(int width, int height);
	

	uint64_t m_FenceValues[Window::BufferCount] = {};

	// Vertex buffer for the cube.
	ComPtr<ID3D12Resource> m_VertexBuffer;
	D3D12_VERTEX_BUFFER_VIEW m_VertexBufferView;
	// Index buffer for the cube.
	ComPtr<ID3D12Resource> m_IndexBuffer;
	D3D12_INDEX_BUFFER_VIEW m_IndexBufferView;

	// Root signature
	ComPtr<ID3D12RootSignature> m_RootSignature;

	// Pipeline state object.
	ComPtr<ID3D12PipelineState> m_PipelineState;

	D3D12_VIEWPORT m_Viewport;
	D3D12_RECT m_ScissorRect;

	float m_FoV;

	DirectX::XMMATRIX m_ModelMatrix;
	DirectX::XMMATRIX m_ViewMatrix;
	DirectX::XMMATRIX m_ProjectionMatrix;

	bool m_ContentLoaded;

private:
	void LoadShaders();
	void CreateGBufferHeaps();
	void CreateGBufferTextures(DirectX::XMINT2 size);
	void CreateCamLightBuffers();
	void LoadMeshes(d3d12::CommandList* cmdList);
	void CreateRootSignatures();
	void CreatePSOs();

private:
	struct
	{
		ComPtr<ID3DBlob> MeshVertexShader;
		ComPtr<ID3DBlob> MeshFragmentShader;
		ComPtr<ID3DBlob> FullscreenVertexShader;
		ComPtr<ID3DBlob> ShadingFragmentShader;
	} Shaders;

	struct
	{
		ComPtr<ID3D12Resource> Albedo;
		ComPtr<ID3D12Resource> Normal;
		ComPtr<ID3D12Resource> Depth;
		ComPtr<ID3D12DescriptorHeap> AlbedoHeap;
		ComPtr<ID3D12DescriptorHeap> NormalHeap;
		ComPtr<ID3D12DescriptorHeap> DepthHeap;
	} GBuffers;
};
}