//! @file renderer.cpp
//! @brief Implementation of Substance renderer class
//! @author Christophe Soum - Allegorithmic
//! @copyright Allegorithmic. All rights reserved.

#include <SubstanceAirGraph.h>

#include "framework/details/detailsrendererimpl.h"
#include "framework/renderer.h"


SubstanceAir::Renderer::Renderer() :
	mRendererImpl(new Details::RendererImpl())
{
}


SubstanceAir::Renderer::~Renderer()
{
	delete mRendererImpl;
}


void SubstanceAir::Renderer::push(SubstanceAir::FGraphInstance* graph)
{
	mRendererImpl->push(graph);
}


void SubstanceAir::Renderer::push(SubstanceAir::List<SubstanceAir::FGraphInstance*>& graphs)
{
	SubstanceAir::List<SubstanceAir::FGraphInstance*>::TIterator
		ItGraph(graphs.itfront());

	for (;ItGraph;++ItGraph)
	{
		push(*ItGraph);
	}
}


UINT SubstanceAir::Renderer::run(UINT runOptions)
{
	return mRendererImpl->run(runOptions);
}


UBOOL SubstanceAir::Renderer::cancel(UINT runUid)
{
	return mRendererImpl->cancel(runUid);
}


void SubstanceAir::Renderer::cancelAll()
{
	mRendererImpl->cancel();
}


UBOOL SubstanceAir::Renderer::isPending(UINT runUid) const
{
	return mRendererImpl->isPending(runUid);
}


void SubstanceAir::Renderer::hold()
{
	mRendererImpl->hold();
}


void SubstanceAir::Renderer::resume()
{
	mRendererImpl->resume();
}


void SubstanceAir::Renderer::setCallbacks(Callbacks* callbacks)
{
	mRendererImpl->setCallbacks(callbacks);
}
