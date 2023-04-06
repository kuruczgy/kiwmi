import { createContext, useState } from 'react'
import styles from './App.module.scss'
import {
  DragDropContext,
  Droppable,
  OnDragEndResponder,
} from 'react-beautiful-dnd'
import { NumberSelector } from './components/NumberSelector'
import _ from 'lodash'
import type { State } from './model'
import { NewWorkspace, Workspace } from './components/Workspace'

type Context = [
  state: State,
  setState: (f: State | ((prev: State) => State)) => void,
]
export const StateContext = createContext<Context>(undefined as any)

const App = () => {
  const [state, setState] = useState<State>({
    state: {
      workspaces: [],
      outputs: [],
      views: {},
    },
    aux: {
      views: {},
    },
  })

  const [ws, setWs] = useState(() => {
    const ws = new WebSocket('ws://localhost:8000', 'main')
    ws.onmessage = (ev) => {
      const data = JSON.parse(ev.data)
      if (data.kind == 'layout_state') {
        console.log('recv state:', data.data)

        // HACK: empty tables from lua get serialized as arrays, fix it
        const state: State = data.data
        for (const key of Object.keys(state.state.views)) {
          if (Array.isArray(state.state.views[key].tags)) {
            state.state.views[key].tags = {}
          }
        }

        setState(data.data)
      }
    }
    return ws
  })

  const setAndSendState = (f: State | ((_: State) => State)) => {
    setState((s) => {
      const newState = typeof f === 'function' ? f(s) : f
      const message = { kind: 'set_layout_state', state: newState.state }
      console.log('sending message:', message)
      ws.send(JSON.stringify(message))
      return newState
    })
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
          top_k: -1,
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
    setAndSendState(newState)
  }

  const onMaxViewsChange = (output_name) => (value) => {
    const nextState = _.cloneDeep(state)
    nextState.state.outputs.find((i) => i.name == output_name)!.max_views =
      _.clamp(value, 1, 8)
    setAndSendState(nextState)
  }

  return (
    <StateContext.Provider value={[state, setAndSendState]}>
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
                  <Workspace key={ws.id} ws={ws} index={index} />
                ))}
                {provided.placeholder}
                <NewWorkspace />
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
    </StateContext.Provider>
  )
}

export default App
