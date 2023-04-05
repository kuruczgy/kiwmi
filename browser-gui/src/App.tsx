import React, { Component, useState } from 'react'
import styles from './App.module.scss'
import {
  DragDropContext,
  Draggable,
  Droppable,
  OnDragEndResponder,
} from 'react-beautiful-dnd'
import NumberSelector from './components/NumberSelector'
import _ from 'lodash'
import { DraggableProvided } from 'react-beautiful-dnd'
import { DraggableStateSnapshot } from 'react-beautiful-dnd'

type ViewInfo = {
  title: string
}
type OutputInfo = {
  name: string
  max_views: number
}
type State = {
  state: {
    workspaces: {
      id: string
      views: string[]
    }[]
    outputs: OutputInfo[]
  }
  views: Record<string, ViewInfo>
}

const draggableProps = (
  provided: DraggableProvided,
  snapshot: DraggableStateSnapshot,
) => ({
  ...provided.draggableProps,
  ...provided.dragHandleProps,
  style: {
    ...provided.draggableProps.style,
    ...(snapshot.isDropAnimating && {
      transitionDuration: '0.001s',
    }),
  },
})

const View = ({
  view_id,
  index,
  info,
}: {
  view_id: string
  index: number
  info: ViewInfo
}) => {
  return (
    <Draggable draggableId={`draggable-view-${view_id}`} index={index}>
      {(provided, snapshot) => (
        <div
          {...draggableProps(provided, snapshot)}
          ref={provided.innerRef}
          className={styles.view}
        >
          <div className={styles.bubble}>
            <p>{info.title}</p>
          </div>
        </div>
      )}
    </Draggable>
  )
}

const clamp = (num: number, min: number, max: number) =>
  Math.min(Math.max(num, min), max)

const App = () => {
  const [state, setState] = useState<State>({
    state: {
      workspaces: [],
      outputs: [],
    },
    views: {},
  })

  const [ws, setWs] = useState(() => {
    const ws = new WebSocket('ws://localhost:8000', 'main')
    ws.onmessage = (ev) => {
      const data = JSON.parse(ev.data)
      if (data.kind == 'layout_state') {
        console.log('recv state:', data.data)
        setState(data.data)
      }
    }
    return ws
  })

  const updateState = (newState) => {
    setState(newState)
    console.log('sending state:', newState.state)
    ws.send(JSON.stringify({ kind: 'set_layout_state', state: newState.state }))
  }

  const onDragEnd: OnDragEndResponder = (result) => {
    if (!result.destination) return

    const newState = _.cloneDeep(state)

    if (result.type === 'view') {
      const workspaceFromIdx = newState.state.workspaces.findIndex(
        (i) => `droppable-ws-${i.id}` == result.source.droppableId,
      )!
      const [item] = newState.state.workspaces[workspaceFromIdx].views.splice(
        result.source.index,
        1,
      )
      if (result.destination.droppableId === 'new-ws') {
        let newId = 1
        while (
          newState.state.workspaces.findIndex(
            (i) => i.id === newId.toString(),
          ) !== -1
        )
          ++newId

        newState.state.workspaces.push({
          id: newId.toString(),
          views: [item],
        })
      } else {
        newState.state.workspaces
          .find(
            (i) => `droppable-ws-${i.id}` == result.destination!.droppableId,
          )!
          .views.splice(result.destination.index, 0, item)
      }

      if (newState.state.workspaces[workspaceFromIdx].views.length === 0) {
        // remove empty workspace
        newState.state.workspaces.splice(workspaceFromIdx, 1)
      }
    } else if (result.type === 'workspace') {
      const [item] = newState.state.workspaces.splice(result.source.index, 1)
      newState.state.workspaces.splice(result.destination.index, 0, item)
    }
    updateState(newState)
  }

  const onMaxViewsChange = (output_name) => (value) => {
    const nextState = _.cloneDeep(state)
    nextState.state.outputs.find((i) => i.name == output_name)!.max_views =
      clamp(value, 1, 8)
    updateState(nextState)
  }

  return (
    <DragDropContext onDragEnd={onDragEnd}>
      <div className={styles.container}>
        <Droppable
          droppableId="workspaces"
          type="workspace"
          direction="horizontal"
        >
          {(provided) => (
            <div
              ref={provided.innerRef}
              {...provided.droppableProps}
              className={styles.workspaces}
            >
              {state.state.workspaces.map((ws, index) => (
                <Draggable
                  draggableId={`draggable-ws-${ws.id}`}
                  index={index}
                  key={ws.id}
                >
                  {(provided, snapshot) => (
                    <div
                      {...draggableProps(provided, snapshot)}
                      ref={provided.innerRef}
                      className={styles.workspace}
                    >
                      <span>{ws.id}</span>
                      <Droppable
                        droppableId={`droppable-ws-${ws.id}`}
                        type="view"
                      >
                        {(provided) => (
                          <div
                            ref={provided.innerRef}
                            {...provided.droppableProps}
                          >
                            {ws.views.map((view_id, index) => (
                              <View
                                key={view_id}
                                view_id={view_id}
                                index={index}
                                info={state.views[view_id]}
                              />
                            ))}
                            {provided.placeholder}
                          </div>
                        )}
                      </Droppable>
                    </div>
                  )}
                </Draggable>
              ))}
              {provided.placeholder}
              <div className={styles.workspace}>
                <span>[new workspace]</span>
                <Droppable droppableId="new-ws" type="view">
                  {(provided) => (
                    <div ref={provided.innerRef} {...provided.droppableProps}>
                      {provided.placeholder}
                    </div>
                  )}
                </Droppable>
              </div>
            </div>
          )}
        </Droppable>
        <div className={styles.outputs}>
          {state.state.outputs.map((output_info, index) => {
            return (
              <NumberSelector
                key={output_info.name}
                label={output_info.name}
                value={output_info.max_views}
                onChange={onMaxViewsChange(output_info.name)}
              />
            )
          })}
        </div>
      </div>
    </DragDropContext>
  )
}

export default App
