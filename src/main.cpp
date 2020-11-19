//#include <bx/platform.h>
//
//#if BX_PLATFORM_LINUX || BX_PLATFORM_BSD
//#	if ENTRY_CONFIG_USE_WAYLAND
//#		include <wayland-egl.h>
//#	endif
//#endif

#include <SDL.h>
#include <SDL_main.h>

#include <bgfx/bgfx.h>
#include <bgfx/platform.h>

#include "utils/bgfx_utils.h"

#include <vector>

#define GLM_FORCE_LEFT_HANDED 1
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

//BX_PRAGMA_DIAGNOSTIC_PUSH()
//BX_PRAGMA_DIAGNOSTIC_IGNORED_CLANG("-Wextern-c-compat")
#include <SDL2/SDL_syswm.h>
//BX_PRAGMA_DIAGNOSTIC_POP()

#include <FastNoise/FastNoise.h>

struct PosColorVertex
{
	float m_x;
	float m_y;
	float m_z;
	uint32_t m_abgr;

	static void init()
	{
		ms_layout
			.begin()
			.add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
			.add(bgfx::Attrib::Color0, 4, bgfx::AttribType::Uint8, true)
			.end();
	};

	static bgfx::VertexLayout ms_layout;
};

bgfx::VertexLayout PosColorVertex::ms_layout;

static PosColorVertex s_cubeVertices[8] =
{
	{-0.5f,  0.5f,  0.5f, 0xff000000 },
	{ 0.5f,  0.5f,  0.5f, 0xff0000ff },
	{-0.5f, -0.5f,  0.5f, 0xff00ff00 },
	{ 0.5f, -0.5f,  0.5f, 0xff00ffff },
	{-0.5f,  0.5f, -0.5f, 0xffff0000 },
	{ 0.5f,  0.5f, -0.5f, 0xffff00ff },
	{-0.5f, -0.5f, -0.5f, 0xffffff00 },
	{ 0.5f, -0.5f, -0.5f, 0xffffffff },
};

static const uint16_t s_cubeIndices[36] =
{
	0, 1, 2, // 0
	1, 3, 2,
	4, 6, 5, // 2
	5, 6, 7,
	0, 2, 4, // 4
	4, 2, 6,
	1, 5, 3, // 6
	5, 7, 3,
	0, 4, 1, // 8
	4, 5, 1,
	2, 3, 6, // 10
	6, 3, 7,
};

static const uint16_t s_cubeTriStrip[] =
{
	0, 1, 2,
	3,
	7,
	1,
	5,
	0,
	4,
	2,
	6,
	7,
	4,
	5,
};

static const uint16_t s_cubeLineList[] =
{
	0, 1,
	0, 2,
	0, 4,
	1, 3,
	1, 5,
	2, 3,
	2, 6,
	3, 7,
	4, 5,
	4, 6,
	5, 7,
	6, 7,
};

static const uint16_t s_cubeLineStrip[] =
{
	0, 2, 3, 1, 5, 7, 6, 4,
	0, 2, 6, 4, 5, 7, 3, 1,
	0,
};

static const uint16_t s_cubePoints[] =
{
	0, 1, 2, 3, 4, 5, 6, 7
};

#define INDEX_ARRAY s_cubeIndices
#define INDEX_PT 0 // BGFX_STATE_PT_TRISTRIP

static void* sdlNativeWindowHandle(SDL_Window* _window, const SDL_SysWMinfo* wmi) {
#	if BX_PLATFORM_LINUX || BX_PLATFORM_BSD
#		if ENTRY_CONFIG_USE_WAYLAND
	wl_egl_window* win_impl = (wl_egl_window*)SDL_GetWindowData(_window, "wl_egl_window");
	if (!win_impl)
	{
		int width, height;
		SDL_GetWindowSize(_window, &width, &height);
		struct wl_surface* surface = wmi->info.wl.surface;
		if (!surface)
			return nullptr;
		win_impl = wl_egl_window_create(surface, width, height);
		SDL_SetWindowData(_window, "wl_egl_window", win_impl);
	}
	return (void*)(uintptr_t)win_impl;
#		else
	return (void*)wmi->info.x11.window;
#		endif
#	elif BX_PLATFORM_OSX
	return wmi->info.cocoa.window;
#	elif BX_PLATFORM_WINDOWS
	return wmi->info.win.window;
#	elif BX_PLATFORM_EMSCRIPTEN
	static char id[] = "#canvas";
	return id;
#	endif // BX_PLATFORM_
}

inline bool sdlSetWindow(SDL_Window* _window) {
	SDL_SysWMinfo* pwmi = nullptr;

#if BX_PLATFORM_EMSCRIPTEN
	// Emscripten is special in that it does not need
	// and will not provide any SDL_SysWMinfo.
#else
	SDL_SysWMinfo wmi;
	SDL_VERSION(&wmi.version);
	if (!SDL_GetWindowWMInfo(_window, &wmi))
		return false;
	pwmi = &wmi;
#endif

	bgfx::PlatformData pd;
	memset(&pd, 0, sizeof(pd));
#	if BX_PLATFORM_LINUX || BX_PLATFORM_BSD
#		if ENTRY_CONFIG_USE_WAYLAND
	pd.ndt = wmi.info.wl.display;
#		else
	pd.ndt = wmi.info.x11.display;
#		endif
#	endif // BX_PLATFORM_

	pd.nwh = sdlNativeWindowHandle(_window, pwmi);
	bgfx::setPlatformData(pd);
	return true;
}

static void sdlDestroyWindow(SDL_Window* _window)
{
	if (!_window)
		return;
#	if BX_PLATFORM_LINUX || BX_PLATFORM_BSD
#		if ENTRY_CONFIG_USE_WAYLAND
	wl_egl_window* win_impl = (wl_egl_window*)SDL_GetWindowData(_window, "wl_egl_window");
	if (win_impl)
	{
		SDL_SetWindowData(_window, "wl_egl_window", nullptr);
		wl_egl_window_destroy(win_impl);
	}
#		endif
#	endif
	SDL_DestroyWindow(_window);
}

#include <chrono>
#include <random>

void initBgfx() {
	bgfx::RendererType::Enum renderers[bgfx::RendererType::Count];
	const int rendererCount = bgfx::getSupportedRenderers(bgfx::RendererType::Count, renderers);
	for (int i = 0; i < rendererCount; ++i)
		printf("Supports: %s\n", bgfx::getRendererName(renderers[i]));

	std::random_device dev;
	std::mt19937 rng(dev());
	std::uniform_int_distribution<std::mt19937::result_type> dist(0, rendererCount - 1);

	bgfx::Init init;
	do {
		init.type = renderers[dist(rng)];
	} while (init.type == bgfx::RendererType::Noop && rendererCount > 1);
	init.resolution.width = 1000;
	init.resolution.height = 1000;
	init.resolution.reset = 0;
	bgfx::init(init);

	printf("Rendering with: %s\n", bgfx::getRendererName(init.type));

	// Enable debug text.
	bgfx::setDebug(BGFX_DEBUG_TEXT | BGFX_DEBUG_STATS);

	// Set view 0 clear state.
	bgfx::setViewClear(0, BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH, 0x303030ff, 1.0f, 0);
}

#include "lime/execution.h"

struct Position : Cell, glm::vec3 {
	Position() { }
	Position(const glm::vec3& v) : glm::vec3(v) { }
};

struct OriginPosition : Cell, glm::vec3 {
	OriginPosition() { }
	OriginPosition(const glm::vec3& v) : glm::vec3(v) { }
};

struct MainCamera : Cell {
	glm::mat4 transform;
};

#include <glm/gtx/quaternion.hpp>

void renderScene(
	int width,
	int height,
	float time,
	bgfx::VertexBufferHandle vbh,
	bgfx::IndexBufferHandle ibh,
	bgfx::ProgramHandle program,
	ExecutionContext& ec
) {
	if (!bgfx::isValid(program))
		return;

	const glm::vec3 at = { 0.0f, 0.0f,   0.0f };
	const glm::vec3 eye = { 0.0f, -130.0f, 30.0f };

	// Set view and projection matrix for view 0.
	ec.Append(EachRow([width, height](const MainCamera& mc) {
		float proj[16];
		bx::mtxProj(proj, 60.0f, float(width) / float(height), 0.1f, 1000.0f, bgfx::getCaps()->homogeneousDepth);
		bgfx::setViewTransform(0, glm::value_ptr(mc.transform), proj);
	}));

	if (bgfx::getCaps()->supported & BGFX_CAPS_INSTANCING) {
		// 80 bytes stride = 64 bytes for 4x4 matrix + 16 bytes for RGBA color.
		const uint16_t instanceStride = 80;

		bgfx::InstanceDataBuffer idb;
		ec.Append(
			Once([&idb](RowCount rowCount) {
				bgfx::allocInstanceDataBuffer(&idb, (uint32_t)rowCount.value, instanceStride);
			}),
			EachRow([&idb](const Position& p, IterationIndex index) {
				const auto transform = glm::translate(glm::mat4(1), p);

				auto data = idb.data + index.value * instanceStride;
				memcpy(data, glm::value_ptr(transform), sizeof(transform));

				float* color = (float*)&data[64];
				color[0] = 1.0f;
				color[1] = 1.0f;
				color[2] = 1.0f;
				color[3] = 1.0f;
			}),
			Once([&idb, &vbh, &ibh, &program]() {
				// Set vertex and index buffer.
				bgfx::setVertexBuffer(0, vbh);
				bgfx::setIndexBuffer(ibh);

				// Set instance data buffer.
				bgfx::setInstanceDataBuffer(&idb);

				// Set render states.
				bgfx::setState(BGFX_STATE_DEFAULT);

				// Submit primitive for rendering to view 0.
				bgfx::submit(0, program);
			})
		);
	}
	else {
		bgfx::setVertexBuffer(0, vbh);
		bgfx::setIndexBuffer(ibh);
		bgfx::setState(BGFX_STATE_DEFAULT | INDEX_PT);
		ec.Append(EachRow([program](const Position& p) {
			const auto transform = glm::translate(glm::mat4(1), p);
			bgfx::setTransform(glm::value_ptr(transform));
			bgfx::submit(0, program, 0, BGFX_DISCARD_TRANSFORM);
		}));
		bgfx::discard(BGFX_DISCARD_ALL);
	}
}

#include <algorithm>
#include <random>

#if BX_PLATFORM_EMSCRIPTEN
#	include <emscripten/emscripten.h>
#endif

#include <functional>

void CoreGatheringLogic(ExecutionContext& ec) {
	/**
	 * For each drone with target:
     *    Either set velocity to approach target
     *    or mark as at target
     * Create empty consumption multi map (target -> potential consumers).
     *    Note: Could maybe be done with shared components and a way to query batches
     * For each drone at target:
     *    Insert consumption goal
     * For each [target, consumers] in consumptions:
     *    Store inventory changes in both target and consumers (up to limit)
	 */
}

void main_loop(void* loop) {
	(*static_cast<std::function<bool()>*>(loop))();
}

#if BX_PLATFORM_EMSCRIPTEN
void tryLoadThing(const char* url) {
	em_async_wget2_data_onload_func onload = [](unsigned request, void* arg, void* buffer, unsigned bytes) {
		printf("onload(%s) = %u bytes\n", (const char*)arg, bytes);
	};
	em_async_wget2_data_onerror_func onerror = [](unsigned request, void* arg, int httpError, const char* statusDescription) {
		printf("onerror(%s) = %d, %s\n", (const char*)arg, httpError, statusDescription);
	};
	em_async_wget2_data_onprogress_func onprogress = [](unsigned request, void* arg, int loadedBytes, int totalBytes) {
		printf("onprogress(%s) = %d/%d bytes\n", (const char*)arg, loadedBytes, totalBytes);
	};
	emscripten_async_wget2_data(url, "GET", "", (void*)url, 1, onload, onerror, onprogress);
}
#endif

void loadMemAsync(const char* url) {
#if BX_PLATFORM_EMSCRIPTEN
	em_async_wget2_data_onload_func onload = [](unsigned request, void* arg, void* buffer, unsigned bytes) {
		printf("onload(%s) = %u bytes\n", (const char*)arg, bytes);
	};
	em_async_wget2_data_onerror_func onerror = [](unsigned request, void* arg, int httpError, const char* statusDescription) {
		printf("onerror(%s) = %d, %s\n", (const char*)arg, httpError, statusDescription);
	};
	em_async_wget2_data_onprogress_func onprogress = [](unsigned request, void* arg, int loadedBytes, int totalBytes) {
		printf("onprogress(%s) = %d/%d bytes\n", (const char*)arg, loadedBytes, totalBytes);
	};
	emscripten_async_wget2_data(url, "GET", "", (void*)url, 1, onload, onerror, onprogress);
#endif
}

#include <chrono>

int main(int argc, char* argv[]) {
    SDL_Init(SDL_INIT_EVENTS);
    auto win = SDL_CreateWindow(
        "Title",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        1000, 1000,
        SDL_WINDOW_RESIZABLE
    );
	sdlSetWindow(win);

	initBgfx();

	// Create vertex stream declaration.
	PosColorVertex::init();

	auto vbh = bgfx::createVertexBuffer(
		bgfx::makeRef(s_cubeVertices, sizeof(s_cubeVertices)),
		PosColorVertex::ms_layout
	);

	// Create static index buffer.
	auto ibh = bgfx::createIndexBuffer(
		bgfx::makeRef(INDEX_ARRAY, sizeof(INDEX_ARRAY))
	);

	initBgfxUtils();

	// Load the program
	bgfx::ProgramHandle program = BGFX_INVALID_HANDLE;
	if (bgfx::getCaps()->supported & BGFX_CAPS_INSTANCING)
		program = loadProgram("vs_instancing", "fs_instancing");
	else
		program = loadProgram("vs_cubes", "fs_cubes");

	const auto extent = bgfx::getCaps()->supported & BGFX_CAPS_INSTANCING ? 50 : 10;
	ExecutionContext ec;

	const glm::vec3 at = { 0.0f, 0.0f,   0.0f };
	const glm::vec3 eye = { 0.0f, -130.0f, 100.0f };
	ec.db.Insert(
		MainCamera{ .transform = glm::lookAt(eye, at, glm::vec3(0, 0, 1)) }
	);

	FastNoise noise(123456);
	for (int y = -extent; y <= extent; ++y) {
		for (int x = -extent; x <= extent; ++x) {
			const auto height = int(noise.GetPerlin(x * 3.0f, y * 3.0f) * 20.0f);

			for (int z = 0; z < 3; ++z) {
				const glm::vec3 pos = { x * 1.0f, y * 1.0f, (float)(height + z) };
				ec.db.Insert(OriginPosition(pos), Position(pos));
			}
		}
	}

	const auto start = std::chrono::steady_clock::now();
	auto prev = start;

	auto loop = [&] {
		typedef std::chrono::duration<float> fsec;
		const auto now = std::chrono::steady_clock::now();
		const fsec totalTime = now - start;
		const fsec elapsed = now - prev;
		prev = now;

		SDL_Event event;
		while (SDL_PollEvent(&event)) {
			switch (event.type) {
			case SDL_QUIT:
				return false;
			case SDL_WINDOWEVENT: {
				switch (event.window.event) {
					case SDL_WINDOWEVENT_SIZE_CHANGED:
						bgfx::reset(event.window.data1, event.window.data2);
						break;
					}
				}
				break;
			}
		}

		ec.Append(EachRow([elapsed](MainCamera& mc) {
			auto keyboardState = SDL_GetKeyboardState(nullptr);
			const auto dist = elapsed.count() * 5;
			
			const auto inverted = glm::inverse(mc.transform);
			const auto forward = normalize(glm::vec3(inverted[2]));
			if (keyboardState[SDL_SCANCODE_W] || keyboardState[SDL_SCANCODE_UP])
				mc.transform = glm::translate(mc.transform, forward * -dist);
			if (keyboardState[SDL_SCANCODE_S] || keyboardState[SDL_SCANCODE_DOWN])
				mc.transform = glm::translate(mc.transform, forward * dist);
		}));

		int w, h;
		SDL_GetWindowSize(win, &w, &h);

		// Set view 0 default viewport.
		bgfx::setViewRect(0, 0, 0, uint16_t(w), uint16_t(h));

		// This dummy draw call is here to make sure that view 0 is cleared
		// if no other draw calls are submitted to view 0.
		bgfx::touch(0);

		ec.Append(EachRow([totalTime](Position& p, const OriginPosition& o) {
			p = o;
			p.z += std::sinf(totalTime.count() * 1.125354 + p.x * 0.1) * std::cosf(totalTime.count() * 0.15495 + p.y * 0.2) * 3;
		}));

		renderScene(w, h, 0, vbh, ibh, program, ec);

		bgfx::frame();
		return true;
	};

#if BX_PLATFORM_EMSCRIPTEN
	tryLoadThing("http://10.92.1.18:5000/silly.jpg");
	std::function<bool()> wrappedLoop = loop;
	emscripten_set_main_loop_arg(main_loop, &wrappedLoop, 0, 1);
#else
	while (loop());
#endif

	bgfx::shutdown();
    sdlDestroyWindow(win);
	SDL_Quit();
    return 0;
}